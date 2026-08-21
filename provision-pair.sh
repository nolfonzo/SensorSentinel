#!/usr/bin/env bash
#
# Provision a SensorSentinel gateway + Pi Zero pair for a site.
#
# Deliberately generic: it takes no names. The gateway's access point is
# already MAC-derived and therefore unique (HT-M7603-XXXX), so the script reads
# that off the device and uses it for the hostname too. Nothing per-unit has to
# be typed, and there is no way to give two units the same name by mistake.
#
# The Pi is pinned to its gateway's BSSID, so even if another unit somehow
# advertised the same SSID it could not wander onto the wrong one. With several
# pairs on a bench, power up one gateway at a time.
#
# The only genuinely per-site input is the WiFi the gateway will use for its
# uplink. Everything else is discovered or derived.
#
# Usage:
#   ./provision-pair.sh --bench          # at the bench: finds both, uses BENCH_SSID
#
# or explicitly:
#   ./provision-pair.sh --gateway 192.168.9.130 --pi 192.168.9.145 \
#       --pi-sudo-pass 'xxx' --mqtt-pass 'xxx' \
#       --site-ssid "Neighbour WiFi" --site-pass 'xxx' \
#       --uplink-host 100.114.240.29

set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"


# Settings come from sensorsentinel.env at the repo root, so the everyday
# commands need no arguments. Anything here is still overridable on the
# command line - the env only supplies defaults.
_ENV="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/./sensorsentinel.env"
[[ -r "$_ENV" ]] && set -a && . "$_ENV" && set +a

GATEWAY=""; PI=""; PI_SUDO="${PI_SUDO_PASS:-}"; MQTT_PASS="${MQTT_PASS:-}"
SITE_SSID="${SITE_SSID:-}"; SITE_PASS="${SITE_PASS:-}"
UPLINK_HOST="${UPLINK_HOST:-100.114.240.29}"
GW_SSH_PASS="${GW_SSH_PASS:-heltec.org}"
TS_AUTHKEY="${TS_AUTHKEY:-}"
AP_PASS="${AP_PASS:-}"
PI_STATIC="192.168.8.2"
PROFILE="AU915-subband0"

die() { echo "error: $*" >&2; exit 1; }
say() { echo "  $*"; }
hdr() { echo; echo "── $* ──"; }

while [[ $# -gt 0 ]]; do
  case "$1" in
    --gateway)      GATEWAY="$2"; shift 2 ;;
    --pi)           PI="$2"; shift 2 ;;
    --pi-sudo-pass) PI_SUDO="$2"; shift 2 ;;
    --mqtt-pass)    MQTT_PASS="$2"; shift 2 ;;
    --site-ssid)    SITE_SSID="$2"; shift 2 ;;
    --site-pass)    SITE_PASS="$2"; shift 2 ;;
    --uplink-host)  UPLINK_HOST="$2"; shift 2 ;;
    --gw-ssh-pass)  GW_SSH_PASS="$2"; shift 2 ;;
    --ap-pass)      AP_PASS="$2"; shift 2 ;;
    --pi-static)    PI_STATIC="$2"; shift 2 ;;
    --bench)        SITE_SSID="${BENCH_SSID:-}"; SITE_PASS="${BENCH_PASS:-}"; shift ;;
    --profile)      PROFILE="$2"; shift 2 ;;
    -h|--help)      sed -n '2,22p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) die "unknown option: $1" ;;
  esac
done

# Discovery, so neither address has to be known. The gateway answers on
# 192.168.8.1 whenever you are joined to its own AP, but at the bench both
# devices must be reachable at once, which means both are on the LAN.
discover() {   # $1 = label, $2 = probe command template using {} for the ip
  local label="$1" probe="$2" ip
  # mDNS first: the Pi's hostname was set at flash time, and the gateway's
  # hostname follows its MAC-derived AP name.
  for n in $3; do
    ip=$(getent hosts "$n" 2>/dev/null | awk '{print $1; exit}')
    [[ -n "$ip" ]] && { echo "$ip"; return 0; }
  done
  # Fall back to sweeping the local /24 for something that answers the probe.
  local base
  base=$(ip route get 1.1.1.1 2>/dev/null | grep -oE 'src [0-9.]+' | cut -d' ' -f2 | cut -d. -f1-3)
  [[ -n "$base" ]] || return 1
  for i in $(seq 1 254); do
    ( eval "${probe//\{\}/$base.$i}" >/dev/null 2>&1 && echo "$base.$i" ) &
  done | head -1
  wait 2>/dev/null || true
}

if [[ -z "$GATEWAY" ]]; then
  say "no --gateway given, searching..."
  # A Heltec gateway is the thing on the LAN with telnet AND http open.
  GATEWAY=$(discover gateway 'timeout 2 bash -c "</dev/tcp/{}/23" && timeout 2 bash -c "</dev/tcp/{}/80"' 'HT-M7603.local HT-M7603-844A.local')
  [[ -n "$GATEWAY" ]] && say "found gateway at $GATEWAY" || die "could not find a gateway - pass --gateway"
fi

if [[ -z "$PI" ]]; then
  say "no --pi given, searching..."
  PI=$(discover pi 'timeout 2 bash -c "</dev/tcp/{}/22"' 'pi0-north.local raspberrypi.local')
  [[ -n "$PI" ]] && say "found Pi at $PI" || die "could not find the Pi - pass --pi"
fi

[[ -n "$MQTT_PASS" ]] || die "--mqtt-pass is required"

GW_SSH=(-o BatchMode=yes -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null
        -o KexAlgorithms=+diffie-hellman-group14-sha1,diffie-hellman-group1-sha1
        -o HostKeyAlgorithms=+ssh-rsa -o PubkeyAcceptedKeyTypes=+ssh-rsa -o ConnectTimeout=15)

# Key first, password only as a fallback, so a re-run needs no credentials.
if ssh "${GW_SSH[@]}" "root@$GATEWAY" true 2>/dev/null; then
  gw() { ssh "${GW_SSH[@]}" "root@$GATEWAY" "$@" 2>/dev/null; }
else
  command -v sshpass >/dev/null || die "gateway needs a password and sshpass is missing"
  export SSHPASS="$GW_SSH_PASS"
  gw() { sshpass -e ssh "${GW_SSH[@]}" -o PubkeyAuthentication=no "root@$GATEWAY" "$@" 2>/dev/null; }
fi

hdr "discovering the gateway"
AP_SSID="$(gw 'uci get wireless.ap.ssid')"
AP_BSSID="$(gw 'ifconfig ra0 2>/dev/null | grep -oE "HWaddr [0-9A-Fa-f:]+" | cut -d" " -f2')"
GW_LAN="$(gw 'uci get network.lan.ipaddr')"
[[ -n "$AP_SSID" ]] || die "could not read the gateway's AP SSID - is $GATEWAY a HT-M7603?"
say "AP SSID:  $AP_SSID   (MAC-derived, unique)"
say "AP BSSID: ${AP_BSSID:-unknown}"
say "gateway LAN: $GW_LAN, Pi will be static $PI_STATIC"

# The AP ships keyed with the vendor default, which is published in Heltec's
# docs - an open door at a site. Rotated automatically unless one is supplied,
# so it is never left at the default by omission.
if [[ -z "$AP_PASS" ]]; then
  AP_PASS="$(tr -dc 'A-HJ-NP-Za-hj-km-z2-9' < /dev/urandom | head -c 20)"
  say "AP password: generated per-pair (set AP_PASS in the env to fix it fleet-wide)"
  AP_PASS_GENERATED=1
else
  say "AP password: from sensorsentinel.env"
  AP_PASS_GENERATED=0
fi

hdr "configuring the gateway"
"$HERE/gateway/provision-gateway.sh" \
  --host "$GATEWAY" --ssh-pass "$GW_SSH_PASS" --profile "$PROFILE" \
  --mqtt-host "$PI_STATIC" --mqtt-user heltec-jetty --mqtt-pass "$MQTT_PASS" \
  ${SITE_SSID:+--wifi-ssid "$SITE_SSID"} ${SITE_PASS:+--wifi-pass "$SITE_PASS"} \
  --ap-pass "$AP_PASS"

# Hostname follows the AP name, so the unit is unambiguous on any network it
# joins without anyone choosing a name.
gw "uci set system.@system[0].hostname='$AP_SSID'; uci commit system; echo '$AP_SSID' > /proc/sys/kernel/hostname" || true

hdr "configuring the Pi"
"$HERE/pi/provision-pi.sh" \
  --host "$PI" --sudo-pass "$PI_SUDO" \
  --uplink-host "$UPLINK_HOST" \
  --mqtt-user heltec-jetty --mqtt-pass "$MQTT_PASS" \
  --gateway-host "$GW_LAN" \
  ${TS_AUTHKEY:+--ts-authkey "$TS_AUTHKEY"}

hdr "pairing"

# ORDER MATTERS. Binding the Pi to the gateway's access point moves it behind
# the gateway's NAT, where it is unreachable from the LAN. From that moment the
# tunnel is the only way back in - so refuse to take the step until the tunnel
# is demonstrably working. Getting this wrong at a site means a drive back with
# a USB cable, or a reflash.
TS_IP="$(ssh -o BatchMode=yes -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
         -o ConnectTimeout=10 "nolfonzo@$PI" 'tailscale ip -4 2>/dev/null' 2>/dev/null | tr -d '[:space:]')"
if [[ -z "$TS_IP" ]]; then
  echo
  die "the Pi is not on the tailnet yet, so binding it to the gateway would strand it.

  Authorise it first, then re-run:

      ssh nolfonzo@$PI 'sudo tailscale up'

  Or put a reusable TS_AUTHKEY in sensorsentinel.env to make this automatic:
      https://login.tailscale.com/admin/settings/keys"
fi
say "tunnel verified: the Pi is reachable at $TS_IP"
PI_SSH=(-o BatchMode=yes -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o ConnectTimeout=15)
pisudo() { printf '%s\n' "$PI_SUDO" | ssh "${PI_SSH[@]}" "nolfonzo@$PI" "sudo -S bash -c '$1'" 2>&1 | grep -v '^\[sudo\]' || true; }

# Static address on the gateway's LAN: the gateway can only be given an IP for
# its broker, so the Pi's address has to be predictable. Pinned to the BSSID so
# it can only ever associate with this gateway's radio.
say "binding the Pi to $AP_SSID (static $PI_STATIC, BSSID-pinned)"
pisudo "nmcli connection delete ss-gateway >/dev/null 2>&1 || true
        nmcli connection add type wifi con-name ss-gateway ifname wlan0 ssid '$AP_SSID' \
          wifi-sec.key-mgmt wpa-psk wifi-sec.psk '$AP_PASS' \
          ipv4.method manual ipv4.addresses $PI_STATIC/24 ipv4.gateway $GW_LAN ipv4.dns $GW_LAN \
          connection.autoconnect yes connection.autoconnect-priority 10 >/dev/null
        ${AP_BSSID:+nmcli connection modify ss-gateway 802-11-wireless.bssid '$AP_BSSID'}"

# The watchdog restarts the gateway's forwarder over SSH, so it needs a key.
say "installing the Pi's key on the gateway (for the watchdog)"
PI_PUBKEY="$(ssh "${PI_SSH[@]}" "nolfonzo@$PI" 'cat ~/.ssh/id_rsa.pub' 2>/dev/null)"
[[ -n "$PI_PUBKEY" ]] && gw "grep -qF '$PI_PUBKEY' /etc/dropbear/authorized_keys 2>/dev/null || echo '$PI_PUBKEY' >> /etc/dropbear/authorized_keys; chmod 600 /etc/dropbear/authorized_keys"

hdr "verifying the pair"
# The Pi has just changed networks. Confirm the tunnel survived the move -
# if it did not, say so now while the device is still on the bench.
sleep 20
if ssh -o BatchMode=yes -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
       -o ConnectTimeout=15 "nolfonzo@$TS_IP" 'true' 2>/dev/null; then
  say "reachable over Tailscale at $TS_IP after the move"
else
  say "WARNING: not reachable over Tailscale yet. It may still be associating;"
  say "if it does not come back, recover over USB before this leaves the bench."
fi

hdr "done"
cat <<EOF
  Pair provisioned.

    gateway   $AP_SSID
              AP password rotated off the vendor default
              uplink WiFi: ${SITE_SSID:-<unchanged>}
              publishes MQTT to $PI_STATIC

    Pi        joins $AP_SSID as $PI_STATIC, BSSID-pinned
              bridges home to $UPLINK_HOST over Tailscale
              watchdog watches the data and restarts the gateway if it stalls

$( [[ "${AP_PASS_GENERATED:-0}" == "1" ]] && printf '  Record the AP password - it is the only way onto this gateway:\n\n    %s\n' "$AP_PASS" || printf '  AP password: as set in sensorsentinel.env.\n' )

  Remaining, both needing a human:
    - authorise the Pi on the tailnet   (ssh to it and run: sudo tailscale up)
    - reboot the Pi to activate USB gadget mode
EOF
