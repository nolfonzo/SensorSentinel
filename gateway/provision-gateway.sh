#!/usr/bin/env bash
#
# Provision a Heltec HT-M7603 LoRa gateway to a known-good configuration.
#
# A fresh unit ships configured for US915 with no MQTT mode set, so it hears
# nothing on an Australian node and forwards over UDP rather than MQTT. This
# script brings one to the same state as a known-working gateway, changing only
# what actually differs between sites.
#
# What it sets:
#   1. Radio config from a profile in ./profiles (region, sub-band, frequencies)
#   2. MQTT mode, broker address and credentials in /lora/setting
#   3. The WiFi client SSID/key for the site it is going to
#   4. The gateway's OWN access-point password, off the vendor default
#
# Deliberately NOT done through the web UI: /www/cgi-bin/setgateway starts with
# `cp global_conf_$region.json global_conf.json`, regenerating the radio config
# from the region template every time it is used. Writing the files directly is
# the only way to reproduce a tuned config exactly.
#
# Usage:
#   ./provision-gateway.sh --host 192.168.9.130 \
#       --wifi-ssid "Neighbour WiFi" --wifi-pass "secret" \
#       --mqtt-host 192.168.9.145 --mqtt-user heltec-jetty --mqtt-pass "secret" \
#       --ap-pass "new-ap-password"
#
# --bench points it at your own WiFi (BENCH_SSID in the env) while building a
# unit. --dry-run prints what would change without touching the device.

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"


# Settings come from sensorsentinel.env at the repo root, so the everyday
# commands need no arguments. Anything here is still overridable on the
# command line - the env only supplies defaults.
_ENV="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../sensorsentinel.env"
[[ -r "$_ENV" ]] && set -a && . "$_ENV" && set +a

# 192.168.8.1 is the gateway on its own access point - true on every unit, so
# joining that AP is all you need to reach one.
HOST="${GATEWAY_HOST:-192.168.8.1}"
SSH_USER="root"
SSH_PASS="${GW_SSH_PASS:-heltec.org}"        # vendor default; override for an already-provisioned unit
PROFILE="AU915-subband0"
WIFI_SSID="${SITE_SSID:-}"
WIFI_PASS="${SITE_PASS:-}"
MQTT_HOST="${MQTT_HOST:-192.168.8.2}"
MQTT_PORT="1883"
MQTT_USER="${MQTT_USER:-heltec-jetty}"
MQTT_PASS="${MQTT_PASS:-}"
AP_PASS=""
DRY_RUN=0

die() { echo "error: $*" >&2; exit 1; }
say() { echo "  $*"; }

usage() {
  sed -n '2,30p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
  exit "${1:-0}"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --host)       HOST="$2"; shift 2 ;;
    --ssh-user)   SSH_USER="$2"; shift 2 ;;
    --ssh-pass)   SSH_PASS="$2"; shift 2 ;;
    --profile)    PROFILE="$2"; shift 2 ;;
    --wifi-ssid)  WIFI_SSID="$2"; shift 2 ;;
    --wifi-pass)  WIFI_PASS="$2"; shift 2 ;;
    --mqtt-host)  MQTT_HOST="$2"; shift 2 ;;
    --mqtt-port)  MQTT_PORT="$2"; shift 2 ;;
    --mqtt-user)  MQTT_USER="$2"; shift 2 ;;
    --mqtt-pass)  MQTT_PASS="$2"; shift 2 ;;
    --ap-pass)    AP_PASS="$2"; shift 2 ;;
    --bench)      WIFI_SSID="${BENCH_SSID:-}"; WIFI_PASS="${BENCH_PASS:-}"; shift ;;
    --dry-run)    DRY_RUN=1; shift ;;
    -h|--help)    usage 0 ;;
    *)            die "unknown option: $1 (try --help)" ;;
  esac
done


PROFILE_DIR="$HERE/profiles/$PROFILE"
[[ -d "$PROFILE_DIR" ]] || die "no such profile: $PROFILE (looked in $PROFILE_DIR)"
[[ -f "$PROFILE_DIR/global_conf.json" ]] || die "profile is missing global_conf.json"
[[ -f "$PROFILE_DIR/settings.toml" ]]    || die "profile is missing settings.toml"

# Dropbear on this firmware is from 2015 and only offers algorithms that modern
# OpenSSH refuses by default. It also has no SFTP server, so scp must be forced
# to the legacy protocol with -O.
SSH_OPTS=(
  -o StrictHostKeyChecking=no
  -o UserKnownHostsFile=/dev/null
  -o KexAlgorithms=+diffie-hellman-group14-sha1,diffie-hellman-group1-sha1
  -o HostKeyAlgorithms=+ssh-rsa
  -o PubkeyAcceptedKeyTypes=+ssh-rsa
  -o ConnectTimeout=15
)

# Prefer key auth; fall back to the password only if a key does not work. That
# way an already-provisioned gateway with your key installed needs no password.
if ssh -o BatchMode=yes "${SSH_OPTS[@]}" "$SSH_USER@$HOST" true 2>/dev/null; then
  say "auth: ssh key"
  RSH=(ssh "${SSH_OPTS[@]}")
  RCP=(scp -O "${SSH_OPTS[@]}")
else
  command -v sshpass >/dev/null || die "key auth failed and sshpass is not installed"
  say "auth: password"
  export SSHPASS="$SSH_PASS"
  RSH=(sshpass -e ssh "${SSH_OPTS[@]}")
  RCP=(sshpass -e scp -O "${SSH_OPTS[@]}")
fi

remote() { "${RSH[@]}" "$SSH_USER@$HOST" "$@"; }

MODEL="$(remote 'cat /proc/cpuinfo | grep machine | cut -d: -f2 | tr -d " \r"' 2>/dev/null || true)"
[[ "$MODEL" == "HT-M7603" ]] || die "target reports model '$MODEL', expected HT-M7603"
say "target: $HOST ($MODEL)"
say "profile: $PROFILE"

if [[ $DRY_RUN -eq 1 ]]; then
  echo
  say "--dry-run, nothing will be changed. Current state:"
  remote 'echo "    setting:  $(cat /lora/setting | tr "\n" " ")"
          echo "    toml:     $(cat /lora/settings.toml | tr "\n" " ")"
          echo "    freqs:    $(grep -oE "\"freq\": [0-9]+" /lora/global_conf.json | tr "\n" " ")"
          echo "    sta ssid: $(uci get wireless.sta.ssid 2>/dev/null)"
          echo "    ap ssid:  $(uci get wireless.ap.ssid 2>/dev/null)"'
  exit 0
fi

STAMP="$(date +%Y%m%d-%H%M%S)"
say "backing up current config to /lora/backup-$STAMP.tar.gz"
# gateway_mode matters on 2025 firmware and does not exist on 2022 - include it
# when present, or a restore silently leaves the unit in the wrong mode.
remote "cd / && tar czf /lora/backup-$STAMP.tar.gz lora/setting lora/settings.toml lora/global_conf.json etc/config/wireless \$([ -f /lora/gateway_mode ] && echo lora/gateway_mode) 2>/dev/null || true"

say "pushing radio profile"
"${RCP[@]}" -q "$PROFILE_DIR/global_conf.json" "$SSH_USER@$HOST:/lora/global_conf.json"
"${RCP[@]}" -q "$PROFILE_DIR/settings.toml"    "$SSH_USER@$HOST:/lora/settings.toml"

# /lora/setting is read at start by /etc/init.d/lrgateway. The "mode" line is
# what selects the MQTT build of the forwarder; without it the plain UDP
# lora_pkt_fwd runs instead and nothing reaches the broker.
say "writing /lora/setting (MQTT mode → $MQTT_HOST:$MQTT_PORT)"
# printf, not a heredoc: BusyBox ash only treats the delimiter as a terminator
# when a newline follows it, and over SSH the closing quote swallowed that
# newline - so a literal "EOF" line ended up in the file.
remote "printf '%s\n' '\"mode\":\"MQTT\"' '\"username\":\"$MQTT_USER\"' '\"password\":\"$MQTT_PASS\"' '\"serveraddr\":\"$MQTT_HOST\"' '\"port\":\"$MQTT_PORT\"' > /lora/setting"

# Two firmware generations in the wild, and they choose the forwarder binary
# from different files:
#   older (2022): mode comes from /lora/setting
#   newer (2025): mode comes from /lora/gateway_mode, and it also supports
#                 basicstation; /lora/setting still holds the broker details
# Writing only /lora/setting leaves a newer unit silently running the plain
# UDP forwarder - it looks like it started fine and publishes nothing.
if remote 'test -f /lora/gateway_mode' 2>/dev/null; then
  say "newer firmware detected - also setting /lora/gateway_mode"
  remote "printf '%s\n' '\"mode\":\"MQTT\"' > /lora/gateway_mode"
fi

if [[ -n "$WIFI_SSID" ]]; then
  say "setting WiFi client → SSID '$WIFI_SSID'"
  remote "uci set wireless.sta.ssid='$WIFI_SSID'
          uci set wireless.sta.key='$WIFI_PASS'
          uci set wireless.sta.disabled='0'
          uci commit wireless"
  WIFI_CHANGED=1
fi

# Install our public key so every later run - especially at a site, from a
# laptop that may not have sshpass - authenticates without a password.
PUBKEY="$(cat ~/.ssh/id_rsa.pub 2>/dev/null || cat ~/.ssh/id_ed25519.pub 2>/dev/null || true)"
if [[ -n "$PUBKEY" ]]; then
  say "installing your SSH key on the gateway"
  # Passed on stdin rather than embedded in the command: dropbear rejects an
  # exec request once the command string gets long ("exec request failed on
  # channel 0"), and a public key is long enough to trip it.
  printf '%s\n' "$PUBKEY" | "${RSH[@]}" "$SSH_USER@$HOST" \
    'mkdir -p /etc/dropbear; k=$(cat); grep -qF "$k" /etc/dropbear/authorized_keys 2>/dev/null || echo "$k" >> /etc/dropbear/authorized_keys; chmod 600 /etc/dropbear/authorized_keys' \
    || say "could not install the key (continuing)"
fi

if [[ -n "$AP_PASS" ]]; then
  # The unit broadcasts its own WPA2 network keyed with the vendor default
  # 'heltec.org', which is published in Heltec's documentation. On a jetty that
  # is an open door onto the device, so it is changed as part of provisioning.
  say "changing the gateway's own AP password off the vendor default"
  remote "uci set wireless.ap.key='$AP_PASS'
          uci commit wireless"
fi

# BusyBox here has no `nohup`, and a backgrounded forwarder inherits the SSH
# session's stdout, which keeps the connection open until it exits. Redirecting
# all three streams lets ssh return immediately.
# uci commit only writes config - the radio has to be reloaded or the new SSID
# sits there doing nothing until a reboot. Detached, and deliberately last
# before the forwarder restart: this briefly drops the access point you are
# most likely connected through at a site. It comes straight back.
if [[ "${WIFI_CHANGED:-0}" == "1" ]]; then
  say "applying the WiFi change (your connection may blink)"
  remote '(wifi >/dev/null 2>&1 </dev/null &)' || true
  sleep 25
fi

say "restarting the packet forwarder"
remote '(/etc/init.d/lrgateway restart >/tmp/provision-restart.log 2>&1 </dev/null &) ; sleep 15' || true

echo
say "── verification ──"
remote 'echo "    forwarder: $(ps | grep -v grep | grep -c lora_pkt_fwd_mqtt) process(es)"
        echo "    setting:   $(cat /lora/setting | tr "\n" " ")"
        echo "    region:    $(cat /lora/settings.toml | tr "\n" " ")"
        echo "    freqs:     $(grep -oE "\"freq\": [0-9]+" /lora/global_conf.json | tr "\n" " ")"
        echo "    sta ssid:  $(uci get wireless.sta.ssid 2>/dev/null)"
        echo "    ap ssid:   $(uci get wireless.ap.ssid 2>/dev/null)"
        # BusyBox here has no `ss` applet - it returns nothing however healthy
        # the device is, which once produced a completely wrong diagnosis.
        echo "    broker:    $(netstat -tn 2>/dev/null | grep -c "'"$MQTT_HOST:$MQTT_PORT"'") connection(s) to the Pi"'

echo
say "done. If the WiFi SSID changed, the gateway may take a minute to associate,"
say "and will be on a different address once it does."
