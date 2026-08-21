#!/bin/bash
#
# SensorSentinel jetty watchdog.
#
# The Heltec gateway's packet forwarder does NOT reconnect its MQTT session
# after a network change. The process keeps running, reports healthy to `ps`,
# and publishes nothing - verified on the bench 2026-08-21. Unattended, that is
# silent data loss until somebody physically visits.
#
# So this Pi watches the actual data, not the process, and escalates:
#   no data -> restart the gateway's forwarder -> reboot the gateway -> shout.
#
# It also publishes its own health upstream, so the home end can alert on the
# watchdog going quiet. A monitor that fails silently is worse than none.

set -uo pipefail

CONF=/etc/ss-watchdog.conf
[[ -r "$CONF" ]] && . "$CONF"

GATEWAY_HOST="${GATEWAY_HOST:-192.168.8.1}"
GATEWAY_USER="${GATEWAY_USER:-root}"
MQTT_HOST="${MQTT_HOST:-127.0.0.1}"
MQTT_USER="${MQTT_USER:-heltec-jetty}"
MQTT_PASS="${MQTT_PASS:-}"
DATA_TOPIC="${DATA_TOPIC:-lora/out/sensor}"
HEALTH_TOPIC="${HEALTH_TOPIC:-lora/health/$(hostname)}"
UPLINK_HOST="${UPLINK_HOST:-100.114.240.29}"
WAIT_SECS="${WAIT_SECS:-150}"          # a little over 2 node reporting intervals
STATE_DIR="${STATE_DIR:-/var/lib/ss-watchdog}"
REBOOT_AFTER="${REBOOT_AFTER:-3}"      # consecutive failures before power-cycling

mkdir -p "$STATE_DIR"
FAILFILE="$STATE_DIR/consecutive_failures"
[[ -f "$FAILFILE" ]] || echo 0 > "$FAILFILE"
FAILURES=$(cat "$FAILFILE" 2>/dev/null || echo 0)

log() { echo "[ss-watchdog] $*"; }

SSH_OPTS=(
  -o BatchMode=yes -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null
  -o KexAlgorithms=+diffie-hellman-group14-sha1,diffie-hellman-group1-sha1
  -o HostKeyAlgorithms=+ssh-rsa -o PubkeyAcceptedKeyTypes=+ssh-rsa
  -o ConnectTimeout=10
)
gw() { ssh "${SSH_OPTS[@]}" "$GATEWAY_USER@$GATEWAY_HOST" "$@" 2>/dev/null; }

mqtt_args=(-h "$MQTT_HOST")
[[ -n "$MQTT_USER" ]] && mqtt_args+=(-u "$MQTT_USER")
[[ -n "$MQTT_PASS" ]] && mqtt_args+=(-P "$MQTT_PASS")

# ── 1. local broker ──────────────────────────────────────────────────────────
# Everything else depends on this, so it is checked and repaired first.
BROKER="ok"
if ! systemctl is-active --quiet mosquitto; then
  log "mosquitto is down - restarting"
  systemctl restart mosquitto
  sleep 5
  systemctl is-active --quiet mosquitto || BROKER="failed"
  [[ "$BROKER" == "ok" ]] && BROKER="restarted"
fi

# ── 2. tunnel home ───────────────────────────────────────────────────────────
TAILSCALE="ok"
if ! systemctl is-active --quiet tailscaled; then
  log "tailscaled is down - restarting"
  systemctl restart tailscaled
  sleep 5
  TAILSCALE="restarted"
elif ! tailscale status >/dev/null 2>&1; then
  TAILSCALE="not-connected"
fi

# Is the bridge to the home broker actually established? mosquitto retries on
# its own, so this is reported rather than acted on - but a persistent "down"
# here means readings are queueing locally and not reaching home.
UPLINK="down"
ss -tn 2>/dev/null | grep -q "$UPLINK_HOST:1883" && UPLINK="up"

# ── 3. the gateway ───────────────────────────────────────────────────────────
GATEWAY="unreachable"
if ping -c1 -W3 "$GATEWAY_HOST" >/dev/null 2>&1; then
  GATEWAY="reachable"
fi

# ── 4. the thing that actually matters: is data arriving? ────────────────────
# Checking the data rather than the process is the whole point - the failure
# mode we are guarding against looks perfectly healthy from the process side.
DATA="stale"
if timeout $((WAIT_SECS + 15)) mosquitto_sub "${mqtt_args[@]}" \
       -t "$DATA_TOPIC" -C 1 -W "$WAIT_SECS" >/dev/null 2>&1; then
  DATA="flowing"
fi

# ── 5. escalate ──────────────────────────────────────────────────────────────
ACTION="none"
if [[ "$DATA" == "flowing" ]]; then
  [[ "$FAILURES" -gt 0 ]] && log "data restored after $FAILURES failure(s)"
  echo 0 > "$FAILFILE"
  FAILURES=0
else
  FAILURES=$((FAILURES + 1))
  echo "$FAILURES" > "$FAILFILE"
  log "no data on $DATA_TOPIC for ${WAIT_SECS}s (consecutive failures: $FAILURES)"

  if [[ "$GATEWAY" != "reachable" ]]; then
    # Nothing we can do over the network; the gateway is off or its WiFi is
    # gone. Reported upstream so the far end sees the difference between
    # "gateway silent" and "gateway missing".
    ACTION="gateway-unreachable"
    log "gateway $GATEWAY_HOST is unreachable - cannot intervene"
  elif [[ "$FAILURES" -ge "$REBOOT_AFTER" ]]; then
    ACTION="gateway-reboot"
    log "restarting the forwarder has not helped - rebooting the gateway"
    gw 'reboot' || log "reboot command failed"
    echo 0 > "$FAILFILE"   # give it a clean slate to come back in
  else
    ACTION="forwarder-restart"
    log "restarting the gateway's packet forwarder"
    # No nohup on this BusyBox, and a backgrounded forwarder holds the SSH
    # session open unless all three streams are redirected.
    gw '(/etc/init.d/lrgateway restart >/tmp/watchdog-restart.log 2>&1 </dev/null &)' \
      || log "restart command failed"
  fi
fi

# ── 6. report upstream ───────────────────────────────────────────────────────
# Published on lora/... so it rides the existing bridge home. The home end can
# then alert on this going quiet, which catches the watchdog itself dying.
PAYLOAD=$(printf '{"host":"%s","ts":"%s","data":"%s","gateway":"%s","broker":"%s","tailscale":"%s","uplink":"%s","failures":%d,"action":"%s"}' \
  "$(hostname)" "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$DATA" "$GATEWAY" "$BROKER" "$TAILSCALE" "$UPLINK" "$FAILURES" "$ACTION")

mosquitto_pub "${mqtt_args[@]}" -t "$HEALTH_TOPIC" -m "$PAYLOAD" -r 2>/dev/null \
  || log "could not publish health (local broker down?)"

log "data=$DATA gateway=$GATEWAY broker=$BROKER tailscale=$TAILSCALE uplink=$UPLINK action=$ACTION"
