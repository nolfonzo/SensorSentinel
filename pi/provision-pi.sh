#!/usr/bin/env bash
#
# Provision a Raspberry Pi Zero 2 W as a SensorSentinel jetty relay.
#
# The Heltec gateway cannot do TLS, and a home connection behind CGNAT cannot
# accept inbound connections. So the gateway publishes to this Pi across the
# local network only, and the Pi carries the readings home inside Tailscale -
# both ends dialling out, nothing exposed.
#
# Run this against a freshly flashed Pi that is on your bench WiFi. It is
# idempotent: safe to re-run to pick up changes.
#
# Deliberately no Docker. On 512 MB the daemon alone costs a quarter of RAM for
# what is a shell script and a timer, and it fights the read-only root
# filesystem that stops SD cards dying at unattended sites. Reproducibility
# comes from re-running this script instead.
#
# Usage:
#   ./provision-pi.sh --host 192.168.9.145 --sudo-pass 'xxx' \
#       --uplink-host 100.114.240.29 \
#       --mqtt-user heltec-jetty --mqtt-pass 'xxx' \
#       --gateway-host 192.168.8.1 \
#       --gateway-ap-ssid HT-M7603-844A --gateway-ap-pass 'heltec.org' \
#       --tailscale-name pi0-north

set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"


# Settings come from sensorsentinel.env at the repo root, so the everyday
# commands need no arguments. Anything here is still overridable on the
# command line - the env only supplies defaults.
_ENV="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../sensorsentinel.env"
[[ -r "$_ENV" ]] && set -a && . "$_ENV" && set +a

HOST=""; SSH_USER="nolfonzo"; SUDO_PASS="${PI_SUDO_PASS:-}"
UPLINK_HOST="${UPLINK_HOST:-100.114.240.29}"
MQTT_USER="${MQTT_USER:-heltec-jetty}"; MQTT_PASS="${MQTT_PASS:-}"
TS_AUTHKEY="${TS_AUTHKEY:-}"
GATEWAY_HOST="${GATEWAY_HOST:-192.168.8.1}"
GW_AP_SSID=""; GW_AP_PASS=""
TS_NAME=""
SKIP_TAILSCALE=0

die() { echo "error: $*" >&2; exit 1; }
say() { echo "  $*"; }

while [[ $# -gt 0 ]]; do
  case "$1" in
    --host)             HOST="$2"; shift 2 ;;
    --ssh-user)         SSH_USER="$2"; shift 2 ;;
    --sudo-pass)        SUDO_PASS="$2"; shift 2 ;;
    --uplink-host)      UPLINK_HOST="$2"; shift 2 ;;
    --mqtt-user)        MQTT_USER="$2"; shift 2 ;;
    --mqtt-pass)        MQTT_PASS="$2"; shift 2 ;;
    --gateway-host)     GATEWAY_HOST="$2"; shift 2 ;;
    --gateway-ap-ssid)  GW_AP_SSID="$2"; shift 2 ;;
    --gateway-ap-pass)  GW_AP_PASS="$2"; shift 2 ;;
    --tailscale-name)   TS_NAME="$2"; shift 2 ;;
    --skip-tailscale)   SKIP_TAILSCALE=1; shift ;;
    --ts-authkey)       TS_AUTHKEY="$2"; shift 2 ;;
    -h|--help)          sed -n '2,26p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) die "unknown option: $1" ;;
  esac
done

[[ -n "$HOST" ]]      || die "--host is required"
[[ -n "$MQTT_PASS" ]] || die "--mqtt-pass is required"

SSH=(ssh -o BatchMode=yes -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -o ConnectTimeout=15)
SCP=(scp -o BatchMode=yes -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -q)

remote()  { "${SSH[@]}" "$SSH_USER@$HOST" "$@"; }
# sudo -S so the password arrives on stdin rather than the command line, where
# it would be visible in the remote process list.
rsudo()   { printf '%s\n' "$SUDO_PASS" | "${SSH[@]}" "$SSH_USER@$HOST" "sudo -S bash -c '$1'" 2>&1 | grep -v '^\[sudo\]' || true; }

"${SSH[@]}" "$SSH_USER@$HOST" true 2>/dev/null || die "cannot reach $SSH_USER@$HOST with key auth - install your key first (ssh-copy-id)"

MODEL="$(remote 'tr -d "\0" < /proc/device-tree/model' 2>/dev/null || echo unknown)"
say "target: $HOST  ($MODEL)"
[[ -n "$TS_NAME" ]] || TS_NAME="$(remote hostname)"

# ── packages ────────────────────────────────────────────────────────────────
say "installing packages (slow on a Zero - a few minutes)"
rsudo 'DEBIAN_FRONTEND=noninteractive apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq mosquitto mosquitto-clients' >/dev/null
remote 'command -v tailscale >/dev/null' 2>/dev/null \
  || rsudo 'curl -fsSL https://tailscale.com/install.sh | sh' >/dev/null

# ── broker + bridge ─────────────────────────────────────────────────────────
say "configuring mosquitto (bridge → $UPLINK_HOST)"
sed -e "s|__UPLINK_HOST__|$UPLINK_HOST|g" -e "s|__CLIENTID__|$(remote hostname)-uplink|g" "$HERE/mosquitto-jetty.conf" > /tmp/.jetty.conf
"${SCP[@]}" /tmp/.jetty.conf "$SSH_USER@$HOST:/tmp/jetty.conf"
rm -f /tmp/.jetty.conf
# mosquitto 2.x refuses to start unless the password file is 0700, and it must
# stay owned by the mosquitto user or the daemon cannot read it after dropping
# privileges. Both are load-bearing.
rsudo "mv /tmp/jetty.conf /etc/mosquitto/conf.d/jetty.conf
       touch /etc/mosquitto/passwd
       mosquitto_passwd -b /etc/mosquitto/passwd '$MQTT_USER' '$MQTT_PASS'
       chmod 0700 /etc/mosquitto/passwd
       chown mosquitto:mosquitto /etc/mosquitto/passwd
       systemctl enable --now mosquitto >/dev/null 2>&1
       systemctl restart mosquitto"

# ── watchdog ────────────────────────────────────────────────────────────────
say "installing the watchdog"
"${SCP[@]}" "$HERE/ss-watchdog.sh" "$SSH_USER@$HOST:/tmp/ss-watchdog.sh"
"${SCP[@]}" "$HERE/ss-watchdog.service" "$SSH_USER@$HOST:/tmp/ss-watchdog.service"
"${SCP[@]}" "$HERE/ss-watchdog.timer" "$SSH_USER@$HOST:/tmp/ss-watchdog.timer"
rsudo "install -m 755 /tmp/ss-watchdog.sh /usr/local/bin/ss-watchdog.sh
       install -m 644 /tmp/ss-watchdog.service /etc/systemd/system/ss-watchdog.service
       install -m 644 /tmp/ss-watchdog.timer /etc/systemd/system/ss-watchdog.timer
       rm -f /tmp/ss-watchdog.*
       cat > /etc/ss-watchdog.conf <<EOF
GATEWAY_HOST=$GATEWAY_HOST
MQTT_USER=$MQTT_USER
MQTT_PASS=$MQTT_PASS
UPLINK_HOST=$UPLINK_HOST
EOF
       chmod 600 /etc/ss-watchdog.conf
       systemctl daemon-reload
       systemctl enable --now ss-watchdog.timer >/dev/null 2>&1"

# ── key for the watchdog to act on the gateway ──────────────────────────────
say "generating this Pi's SSH key (so the watchdog can restart the gateway)"
remote '[ -f ~/.ssh/id_rsa ] || ssh-keygen -t rsa -b 2048 -N "" -f ~/.ssh/id_rsa -q'
PI_PUBKEY="$(remote 'cat ~/.ssh/id_rsa.pub')"

# ── USB gadget rescue path ──────────────────────────────────────────────────
# Lets a laptop reach the Pi over a single USB cable when WiFi is unavailable -
# the difference between fixing a site remotely and reflashing an SD card.
say "enabling USB gadget mode (rescue path)"
rsudo 'cd /boot/firmware
       grep -q "modules-load=dwc2,g_ether" cmdline.txt || sed -i "s/\brootwait\b/rootwait modules-load=dwc2,g_ether/" cmdline.txt
       grep -q "dr_mode=peripheral" config.txt || printf "\n# USB gadget mode: laptop can configure this Pi over one USB cable.\ndtoverlay=dwc2,dr_mode=peripheral\n" >> config.txt'

# ── join the gateway's access point ─────────────────────────────────────────
# Added at higher priority than the bench network but WITHOUT removing it, so a
# gateway that is down cannot strand the Pi.
if [[ -n "$GW_AP_SSID" ]]; then
  say "adding the gateway's AP '$GW_AP_SSID' as the preferred network"
  rsudo "nmcli connection delete ss-gateway >/dev/null 2>&1 || true
         nmcli connection add type wifi con-name ss-gateway ifname wlan0 ssid '$GW_AP_SSID' \
           wifi-sec.key-mgmt wpa-psk wifi-sec.psk '$GW_AP_PASS' \
           connection.autoconnect yes connection.autoconnect-priority 10 >/dev/null"
fi

# ── tunnel home ─────────────────────────────────────────────────────────────
if [[ $SKIP_TAILSCALE -eq 0 ]]; then
  if remote 'tailscale status >/dev/null 2>&1'; then
    say "tailscale: already authenticated"
  else
    if [[ -n "$TS_AUTHKEY" ]]; then
      # A pre-authorised key removes the only step that needed a human.
      say "tailscale: joining with auth key (no browser needed)"
      rsudo "tailscale up --hostname=$TS_NAME --authkey='$TS_AUTHKEY'" >/dev/null
    else
    say "tailscale: starting - authorise with the URL below"
    rsudo "(tailscale up --hostname=$TS_NAME > /tmp/tsup.log 2>&1 &) ; sleep 8" >/dev/null
    URL="$(remote 'grep -oE "https://login\.tailscale\.com/[a-zA-Z0-9/]+" /tmp/tsup.log 2>/dev/null | head -1')"
    [[ -n "$URL" ]] && say "AUTHORISE: $URL"
    fi
  fi
fi

# ── verify ──────────────────────────────────────────────────────────────────
echo
say "── verification ──"
remote "echo \"    hostname:   \$(hostname)\"
        echo \"    mosquitto:  \$(systemctl is-active mosquitto)\"
        echo \"    watchdog:   \$(systemctl is-active ss-watchdog.timer) (next: \$(systemctl show ss-watchdog.timer -p NextElapseUSecRealtime --value | cut -d' ' -f2))\"
        echo \"    tailscale:  \$(tailscale ip -4 2>/dev/null || echo 'not authorised yet')\"
        echo \"    uplink:     \$(ss -tn 2>/dev/null | grep -c '$UPLINK_HOST:1883') connection(s) to home\"
        echo \"    wifi:       \$(nmcli -t -f NAME,DEVICE connection show --active 2>/dev/null | grep wlan0 | cut -d: -f1)\""

echo
say "Install this Pi's key on its gateway so the watchdog can restart the forwarder:"
echo
echo "    ssh gateway \"echo '$PI_PUBKEY' >> /etc/dropbear/authorized_keys\""
echo
say "USB gadget mode needs a reboot to take effect."
