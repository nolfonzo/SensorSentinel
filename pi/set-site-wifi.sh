#!/bin/bash
#
# Point this Pi's gateway at a site's WiFi. Run it ON the Pi.
#
# This is the only thing that has to happen at a site, and it exists so that it
# can be done from a phone: join the gateway's access point, ssh to the Pi, run
# one command. No laptop, no repo, no checkout.
#
#   set-site-wifi "Their SSID" "their password"
#
# The Pi already holds an SSH key for its gateway - installed during
# provisioning so the watchdog can restart the forwarder - so nothing needs a
# password here.

set -uo pipefail

CONF=/etc/ss-watchdog.conf
[[ -r "$CONF" ]] && . "$CONF"
GATEWAY_HOST="${GATEWAY_HOST:-192.168.8.1}"

SSID="${1:-}"
PASS="${2:-}"

# --scan lists what the GATEWAY can reach, which is the only opinion that
# matters - your phone standing next to it may see networks the gateway's
# aerial cannot. Worth running before typing an SSID at a site.
#
# Note iwlist barely works on this MediaTek radio and reports almost nothing;
# iwpriv's site survey is the one that gives a real answer.
if [[ "$SSID" == "--scan" ]]; then
  GW=(-o BatchMode=yes -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null
      -o LogLevel=ERROR -o KexAlgorithms=+diffie-hellman-group14-sha1,diffie-hellman-group1-sha1
      -o HostKeyAlgorithms=+ssh-rsa -o PubkeyAcceptedKeyTypes=+ssh-rsa -o ConnectTimeout=15)
  echo "  networks the gateway can see:"
  ssh "${GW[@]}" "root@$GATEWAY_HOST" \
    'iwpriv apcli0 set SiteSurvey=1 >/dev/null 2>&1; sleep 5; iwpriv apcli0 get_site_survey 2>/dev/null' \
    | sed 's/^/    /'
  exit 0
fi

if [[ -z "$SSID" || -z "$PASS" ]]; then
  cat <<EOF
usage: set-site-wifi "SSID" "password"
       set-site-wifi --scan        list what the gateway can actually reach

Points the gateway at $GATEWAY_HOST at the named WiFi network, then restarts
the packet forwarder and reports whether it worked.

Quote both arguments - site names very often contain spaces, and an unquoted
one silently becomes two arguments.
EOF
  exit 1
fi

# Options only - the function supplies the ssh itself. (Having "ssh" in here as
# well made it run `ssh ssh -o ... root@...`, which tries to connect to a host
# called "ssh" and reports it as the gateway being unreachable.)
GW=(-o BatchMode=yes -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null
    -o LogLevel=ERROR -o KexAlgorithms=+diffie-hellman-group14-sha1,diffie-hellman-group1-sha1
    -o HostKeyAlgorithms=+ssh-rsa -o PubkeyAcceptedKeyTypes=+ssh-rsa -o ConnectTimeout=15)
gw() { ssh "${GW[@]}" "root@$GATEWAY_HOST" "$@"; }

# Distinguish "not on the network" from "cannot log in" - they need different
# fixes, and one error message for both sends you looking in the wrong place.
if ! ping -c1 -W3 "$GATEWAY_HOST" >/dev/null 2>&1; then
  echo "error: no route to the gateway at $GATEWAY_HOST."
  echo "  Is it powered? Is this Pi on its access point?"
  exit 1
fi
if ! gw true 2>/dev/null; then
  echo "error: the gateway answers but will not accept this Pi's SSH key."
  echo "  Re-run provisioning, or add the key by hand."
  exit 1
fi

echo "  gateway: $(gw 'uci get wireless.ap.ssid' 2>/dev/null)"
echo "  setting its uplink to '$SSID'"

# Values are wrapped in single quotes for the remote shell, so an embedded
# apostrophe would close the quote early and write garbage - losing the
# gateway's uplink entirely. iPhone hotspots are named things like "Carl's 17",
# so this is not hypothetical.
#
# The idiom below is the standard one: ' becomes '\'' - close the quote, an
# escaped literal quote, reopen. The gateway has no base64 to encode around it.
shq() { printf "%s" "${1//\'/\'\\\'\'}"; }
SSID_Q="$(shq "$SSID")"
PASS_Q="$(shq "$PASS")"

gw "uci set wireless.sta.ssid='$SSID_Q'
    uci set wireless.sta.key='$PASS_Q'
    uci set wireless.sta.disabled='0'
    uci commit wireless" || { echo "error: could not write the setting"; exit 1; }

# Read it back before touching the radio. If the value did not land intact,
# reloading now would drop the uplink and strand the gateway on a network that
# does not exist.
WROTE="$(gw 'uci get wireless.sta.ssid' 2>/dev/null)"
if [[ "$WROTE" != "$SSID" ]]; then
  echo "error: the SSID did not store correctly."
  echo "  wanted: [$SSID]"
  echo "  stored: [$WROTE]"
  echo "  Nothing has been applied - the gateway is still on its old network."
  exit 1
fi
echo "  stored correctly: [$WROTE]"

# Reloading the radio drops the access point you are connected through, so this
# is backgrounded on the gateway and we simply wait. The SSH session dies either
# way; that is expected, not a failure.
echo "  applying (your connection to the AP will drop briefly)..."
gw '(wifi >/dev/null 2>&1 </dev/null &)' 2>/dev/null || true
sleep 30

# The forwarder exits when its broker becomes unreachable, and reloading the
# radio does exactly that. Without this the watchdog would fix it within five
# minutes, but there is no reason to leave the site wondering.
echo "  restarting the packet forwarder"
gw '(/etc/init.d/lrgateway restart >/dev/null 2>&1 </dev/null &)' 2>/dev/null || true
sleep 20

echo
echo "  ── result ──"
ASSOC="$(gw 'iwconfig apcli0 2>/dev/null | grep -oE "ESSID:\"[^\"]*\"" | cut -d\" -f2' 2>/dev/null)"
ADDR="$(gw 'ifconfig apcli0 2>/dev/null | grep -oE "inet addr:[0-9.]+" | cut -d: -f2' 2>/dev/null)"
FWD="$(gw 'ps | grep -v grep | grep -c lora_pkt_fwd_mqtt' 2>/dev/null)"

echo "    associated to: ${ASSOC:-NOTHING}"
echo "    gateway got:   ${ADDR:-no address}"
echo "    forwarder:     ${FWD:-0} running"
# Routing through a freshly-associated gateway takes a few seconds to settle,
# so this retries rather than reporting a failure that fixes itself. Checking
# once immediately after the change reports "no internet" on a setup that is
# about to be perfectly fine.
ONLINE="no"
for _ in 1 2 3 4 5 6; do
  if ping -c1 -W3 1.1.1.1 >/dev/null 2>&1; then ONLINE="yes"; break; fi
  sleep 5
done
echo "    this Pi online: $ONLINE"

# Internet is part of success, not a footnote. Without it the Pi cannot reach
# home, and a green "looks good" on a setup that delivers nothing is the worst
# possible outcome - you leave the site believing it works.
if [[ "$ASSOC" == "$SSID" && -n "$ADDR" && "${FWD:-0}" != "0" && "$ONLINE" == "yes" ]]; then
  echo
  echo "  Looks good. Confirm a reading arrives at home before you leave."
else
  echo
  echo "  Something is not right. Check the password, and that the network is in range."
  echo "  Re-run this with the corrected details - it is safe to repeat."
fi
