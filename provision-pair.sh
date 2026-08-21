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
#   ./provision-pair.sh --bench --pi pi0-north     # name it if you have several
#   ./provision-pair.sh --bench                    # or let it find them
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

SSH_USER_PI="${SSH_USER_PI:-nolfonzo}"
NAME=""
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
    --name)         NAME="$2"; shift 2 ;;
    --profile)      PROFILE="$2"; shift 2 ;;
    -h|--help)      sed -n '2,22p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) die "unknown option: $1" ;;
  esac
done

# Names are better than discovery when you know them - and you do, because you
# set the Pi's hostname when you flashed it. Accepts "pi0-north", "pi0-north.local"
# or a bare address, and resolves the first two over mDNS. With several Zeros on
# a bench this is the reliable way to say which one you mean.
resolve_name() {
  local v="$1"
  case "$v" in
    ''|*[!0-9.]*) : ;;              # not a bare IPv4 - fall through and resolve
    *) echo "$v"; return 0 ;;       # looks like an address, use as-is
  esac
  local n="$v"
  case "$n" in *.local) : ;; *) n="$n.local" ;; esac
  local ip
  ip=$(getent hosts "$n" 2>/dev/null | awk '{print $1; exit}')
  [[ -n "$ip" ]] || die "could not resolve '$v' (tried $n) - is it powered up and on the network?"
  # stderr, not stdout: this function's stdout IS the return value, so a
  # progress message here would be captured into the variable alongside the
  # address.
  say "$v → $ip" >&2
  echo "$ip"
}
[[ -n "$GATEWAY" ]] && GATEWAY="$(resolve_name "$GATEWAY")"
[[ -n "$PI" ]]      && PI="$(resolve_name "$PI")"

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
  local found
  found=$(for i in $(seq 1 254); do
    ( eval "${probe//\{\}/$base.$i}" >/dev/null 2>&1 && echo "$base.$i" ) &
  done; wait) 2>/dev/null
  found=$(echo "$found" | grep -v '^$' | sort -u)
  local n
  n=$(echo "$found" | grep -c . )
  # Never guess between candidates. Two gateways on a bench is normal, and
  # silently pairing a Pi to the wrong one produces a setup that looks right
  # and talks to someone else's radio.
  if [[ "$n" -gt 1 ]]; then
    echo "AMBIGUOUS:$(echo "$found" | tr '\n' ' ')"
    return 0
  fi
  echo "$found"
}

if [[ -z "$GATEWAY" ]]; then
  say "no --gateway given, searching..."
  # A gateway already in the register has been done. The one you are holding is
  # the one that is not - so known units are excluded rather than requiring you
  # to remember which address is which.
  KNOWN=""
  if [[ -f "$HERE/INVENTORY.md" ]]; then
    KNOWN=$(awk -F'|' '/^\|/ && NF>3 {gsub(/ /,"",$3); print $3}' "$HERE/INVENTORY.md" \
            | grep -vE '^$|^Gateway$|^-+$')
    [[ -n "$KNOWN" ]] && say "register lists $(echo "$KNOWN" | grep -c .) gateway(s) already provisioned"
  fi
  # A Heltec gateway is the thing on the LAN with telnet AND http open.
  GATEWAY=$(discover gateway 'timeout 2 bash -c "</dev/tcp/{}/23" && timeout 2 bash -c "</dev/tcp/{}/80"' '')
  CANDS="${GATEWAY#AMBIGUOUS:}"
  if [[ "$(echo "$CANDS" | wc -w)" -gt 1 && -n "$KNOWN" ]]; then
    # Ask each candidate its name and drop the ones already recorded.
    # A gateway with both ethernet and WiFi up answers on two addresses and
    # looks like two devices. Identity comes from the radio MAC, which is the
    # one thing that does not change between its interfaces.
    declare -A SEEN=()
    DEDUPED=""
    for c in $CANDS; do
      b=$(ssh -o BatchMode=yes -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR \
            -o KexAlgorithms=+diffie-hellman-group14-sha1,diffie-hellman-group1-sha1 \
            -o HostKeyAlgorithms=+ssh-rsa -o PubkeyAcceptedKeyTypes=+ssh-rsa -o ConnectTimeout=8 \
            "root@$c" 'ifconfig ra0 2>/dev/null | grep -oE "HWaddr [0-9A-Fa-f:]+" | cut -d" " -f2' 2>/dev/null)
      [[ -z "$b" ]] && b=$(SSHPASS="$GW_SSH_PASS" sshpass -e ssh -o StrictHostKeyChecking=no \
            -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -o KexAlgorithms=+diffie-hellman-group14-sha1 \
            -o HostKeyAlgorithms=+ssh-rsa -o PubkeyAuthentication=no -o ConnectTimeout=8 \
            "root@$c" 'ifconfig ra0 2>/dev/null | grep -oE "HWaddr [0-9A-Fa-f:]+" | cut -d" " -f2' 2>/dev/null)
      b="${b:-$c}"
      if [[ -n "${SEEN[$b]:-}" ]]; then
        say "  $c is the same device as ${SEEN[$b]} (radio $b) - ignoring the duplicate"
      else
        SEEN[$b]="$c"
        DEDUPED="$DEDUPED $c"
      fi
    done
    CANDS="$(echo "$DEDUPED" | tr -s ' ' | sed 's/^ //;s/ $//')"

    UNKNOWN=""
    for c in $CANDS; do
      nm=$(ssh -o BatchMode=yes -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR \
            -o KexAlgorithms=+diffie-hellman-group14-sha1,diffie-hellman-group1-sha1 \
            -o HostKeyAlgorithms=+ssh-rsa -o PubkeyAcceptedKeyTypes=+ssh-rsa -o ConnectTimeout=8 \
            "root@$c" 'uci get wireless.ap.ssid' 2>/dev/null)
      [[ -z "$nm" ]] && nm=$(SSHPASS="$GW_SSH_PASS" sshpass -e ssh -o StrictHostKeyChecking=no \
            -o UserKnownHostsFile=/dev/null -o KexAlgorithms=+diffie-hellman-group14-sha1 \
            -o HostKeyAlgorithms=+ssh-rsa -o PubkeyAuthentication=no -o ConnectTimeout=8 \
            "root@$c" 'uci get wireless.ap.ssid' 2>/dev/null)
      if echo "$KNOWN" | grep -qxF "$nm"; then
        say "  skipping $c ($nm) - already in the register"
      else
        UNKNOWN="$UNKNOWN $c"
      fi
    done
    CANDS="$(echo "$UNKNOWN" | tr -s ' ' | sed 's/^ //;s/ $//')"
  fi
  case "$(echo "$CANDS" | wc -w)" in
    0) die "every gateway found is already in the register - pass --gateway to redo one" ;;
    1) GATEWAY="$CANDS" ;;
    *) die "more than one unprovisioned gateway: $CANDS
  Pass --gateway to say which, or power the others down." ;;
  esac
  [[ -n "$GATEWAY" ]] && say "found gateway at $GATEWAY" || die "could not find a gateway - pass --gateway"
fi

if [[ -z "$PI" ]]; then
  say "no --pi given, searching..."
  # "port 22 open" matches every Linux box on the network, so candidates are
  # then asked what they are. Only a Pi Zero answers correctly, which makes
  # this specific rather than merely plausible.
  PI=$(discover pi 'timeout 2 bash -c "</dev/tcp/{}/22"' '')
  PI="${PI#AMBIGUOUS:}"
  CONFIRMED=""
  for cand in $PI; do
    model=$(ssh -o BatchMode=yes -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR \
             -o ConnectTimeout=5 "$SSH_USER_PI@$cand" 'tr -d "\0" < /proc/device-tree/model' 2>/dev/null)
    case "$model" in *"Raspberry Pi Zero"*) CONFIRMED="$CONFIRMED $cand" ;; esac
  done
  CONFIRMED="$(echo "$CONFIRMED" | tr -s ' ' | sed 's/^ //;s/ $//')"
  case "$(echo "$CONFIRMED" | wc -w)" in
    0) die "no Raspberry Pi Zero found - pass --pi" ;;
    1) PI="$CONFIRMED" ;;
    *) die "found more than one Pi Zero: $CONFIRMED
  Pass --pi to say which, or power the others down." ;;
  esac
  [[ -n "$PI" ]] && say "found Pi at $PI" || die "could not find the Pi - pass --pi"
fi

[[ -n "$MQTT_PASS" ]] || die "--mqtt-pass is required"

# NOTE: BatchMode is deliberately NOT in here. It disables password
# authentication outright, so sshpass can never feed the password - it is added
# only to the key-auth probe below, where refusing to prompt is the point.
GW_SSH=(-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR
        -o KexAlgorithms=+diffie-hellman-group14-sha1,diffie-hellman-group1-sha1
        -o HostKeyAlgorithms=+ssh-rsa -o PubkeyAcceptedKeyTypes=+ssh-rsa -o ConnectTimeout=15)

# Key first, password only as a fallback, so a re-run needs no credentials.
if ssh -o BatchMode=yes "${GW_SSH[@]}" "root@$GATEWAY" true 2>/dev/null; then
  say "gateway auth: ssh key"
  gw() { ssh -o BatchMode=yes "${GW_SSH[@]}" "root@$GATEWAY" "$@"; }
else
  command -v sshpass >/dev/null || die "gateway needs a password and sshpass is missing"
  say "gateway auth: password"
  export SSHPASS="$GW_SSH_PASS"
  gw() { sshpass -e ssh "${GW_SSH[@]}" -o PubkeyAuthentication=no "root@$GATEWAY" "$@"; }
fi

# Fail loudly. With stderr suppressed and set -e, a failed read here exited the
# script with no output at all - the hardest kind of failure to diagnose.
gw true >/dev/null 2>&1 || die "cannot reach the gateway at $GATEWAY.
  Check it is powered, and that GW_SSH_PASS in sensorsentinel.env matches
  (a provisioned unit no longer uses the vendor default)."

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

# The gateway takes its name from the Pi it is paired with: pi0-north gives
# gateway-north. Derived rather than typed, so there is nothing to get wrong
# and nothing to remember, but the pair is obvious at a glance - in a phone's
# WiFi list, on the LAN, and in the register.
#
# Falls back to the factory MAC-derived name if the Pi is not named to the
# pi0-<site> pattern, so an oddly-named Pi cannot produce a silly gateway name.
if [[ -z "$NAME" ]]; then
  PI_HOST="$(ssh -o BatchMode=yes -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR \
             -o ConnectTimeout=10 "$SSH_USER_PI@$PI" hostname 2>/dev/null)"
  SITE="${PI_HOST#pi0-}"; SITE="${SITE#pi-}"
  if [[ -n "$SITE" && "$SITE" != "$PI_HOST" ]]; then
    NAME="gateway-$SITE"
    say "naming the gateway '$NAME' (from the Pi's hostname '$PI_HOST')"
  else
    say "Pi hostname '$PI_HOST' is not pi0-<site>, keeping the factory name"
  fi
fi

hdr "configuring the gateway"
"$HERE/gateway/provision-gateway.sh" \
  --host "$GATEWAY" --ssh-pass "$GW_SSH_PASS" --profile "$PROFILE" \
  --mqtt-host "$PI_STATIC" --mqtt-user heltec-jetty --mqtt-pass "$MQTT_PASS" \
  ${SITE_SSID:+--wifi-ssid "$SITE_SSID"} ${SITE_PASS:+--wifi-pass "$SITE_PASS"} \
  --ap-pass "$AP_PASS" ${NAME:+--name "$NAME"} ${GW_ROOT_PASS:+--root-pass "$GW_ROOT_PASS"}

# If it was renamed, the AP the Pi must bind to is the new one - the value read
# before the gateway step is now stale.
[[ -n "$NAME" ]] && AP_SSID="$NAME"

# Hostname follows the AP name, so the unit is unambiguous on any network it
# joins without anyone choosing a name.
# Hostname follows the AP name, so a unit is unambiguous on any network it
# joins without anyone having to choose a name.
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
TS_IP="$(ssh -o BatchMode=yes -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR \
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
PI_SSH=(-o BatchMode=yes -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -o ConnectTimeout=15)
# Everything from here on talks to the Pi over TAILSCALE, not its LAN address.
# The binding step moves the Pi off the house network, which kills any SSH
# session running over that network - including the one issuing the command.
# The result is a script that hangs forever on a step that already succeeded.
# The tunnel survives the move, which is exactly why it is verified first.
PI_ADDR="${TS_IP:-$PI}"
pisudo() { printf '%s\n' "$PI_SUDO" | ssh "${PI_SSH[@]}" "nolfonzo@$PI_ADDR" "sudo -S bash -c '$1'" 2>&1 | grep -v '^\[sudo\]' || true; }

# Static address on the gateway's LAN: the gateway can only be given an IP for
# its broker, so the Pi's address has to be predictable. Pinned to the BSSID so
# it can only ever associate with this gateway's radio.
say "binding the Pi to $AP_SSID (static $PI_STATIC, BSSID-pinned)"
pisudo "nmcli connection delete ss-gateway >/dev/null 2>&1 || true
        nmcli connection add type wifi con-name ss-gateway ifname wlan0 ssid '$AP_SSID' \
          wifi-sec.key-mgmt wpa-psk wifi-sec.psk '$AP_PASS' \
          ipv4.method manual ipv4.addresses $PI_STATIC/24 ipv4.gateway $GW_LAN ipv4.dns $GW_LAN \
          connection.autoconnect yes connection.autoconnect-priority 10 >/dev/null
        ${AP_BSSID:+nmcli connection modify ss-gateway 802-11-wireless.bssid '$AP_BSSID'}
        # Creating the profile is not enough. autoconnect-priority decides which
        # network to pick when connecting; it does not make NetworkManager
        # abandon one that is already working. Without this the gateway moves to
        # 192.168.8.2 and the Pi stays on the house WiFi, so nothing can reach
        # the broker and telemetry stops.
        nmcli connection up ss-gateway >/dev/null 2>&1 || true
        sleep 12"

# The watchdog restarts the gateway's forwarder over SSH, so it needs a key.
say "installing the Pi's key on the gateway (for the watchdog)"
PI_PUBKEY="$(ssh "${PI_SSH[@]}" "nolfonzo@$PI_ADDR" 'cat ~/.ssh/id_rsa.pub' 2>/dev/null)"
[[ -n "$PI_PUBKEY" ]] && gw "grep -qF '$PI_PUBKEY' /etc/dropbear/authorized_keys 2>/dev/null || echo '$PI_PUBKEY' >> /etc/dropbear/authorized_keys; chmod 600 /etc/dropbear/authorized_keys"

# USB gadget mode needs a reboot, and rebooting here does double duty: it also
# proves the pair comes back by itself after a power cut, which is the thing
# that actually happens at an unattended site.
hdr "rebooting the Pi (activates USB rescue mode, and proves it recovers)"
pisudo 'reboot' >/dev/null 2>&1 || true
sleep 45
for i in $(seq 1 20); do
  ssh "${PI_SSH[@]}" "nolfonzo@$PI_ADDR" true 2>/dev/null && break
  sleep 15
done

hdr "verifying the pair"
# The Pi has just changed networks. Confirm the tunnel survived the move -
# if it did not, say so now while the device is still on the bench.
sleep 20
if ssh -o BatchMode=yes -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR \
       -o ConnectTimeout=15 "nolfonzo@$TS_IP" 'true' 2>/dev/null; then
  say "reachable over Tailscale at $TS_IP after the move"
else
  say "WARNING: not reachable over Tailscale yet. It may still be associating;"
  say "if it does not come back, recover over USB before this leaves the bench."
fi

# ── inventory ───────────────────────────────────────────────────────────────
# Written by the script rather than kept by hand, because a hand-kept list is
# accurate right up until the one time it matters. Identifiers only - no
# passwords - so it is safe to commit and share.
INV="$HERE/INVENTORY.md"
if [[ ! -f "$INV" ]]; then
  cat > "$INV" <<'HDR'
# Deployed pairs

Written automatically by `provision-pair.sh`, one row per pair.

LOCAL ONLY - gitignored. It carries WiFi SSIDs next to gateway BSSIDs, which
together can place a site on a map, and it describes one particular network
so it is meaningless to anyone else. Keep it wherever you keep
`sensorsentinel.env`; both are worth having if you rebuild this laptop.

| Provisioned | Gateway | Gateway BSSID | Pi | Pi Tailscale | Uplink WiFi |
|---|---|---|---|---|---|
HDR
fi
# Re-provisioning the same pair should update the register, not grow it. A
# duplicate row would also make the discovery skip count wrong.
if [[ -f "$INV" ]] && grep -q "| $AP_SSID |" "$INV"; then
  say "already in INVENTORY.md - leaving the existing row"
else
printf '| %s | %s | %s | %s | %s | %s |\n' \
  "$(date +%Y-%m-%d)" "$AP_SSID" "${AP_BSSID:-?}" \
  "$(ssh "${PI_SSH[@]}" "nolfonzo@$PI_ADDR" hostname 2>/dev/null || echo '?')" \
  "${TS_IP:-?}" "${SITE_SSID:-<unchanged>}" >> "$INV"
say "recorded in INVENTORY.md"
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

  Nothing left to do here. At the site, join $AP_SSID and run:

    ./gateway/provision-gateway.sh --wifi-ssid "<their SSID>" --wifi-pass "<their password>"
EOF
