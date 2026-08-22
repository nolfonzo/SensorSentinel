# Deploying a gateway + Pi pair

A pair is one Heltec HT-M7603 LoRa gateway and one Raspberry Pi Zero 2 W. The
gateway hears the boat node over LoRa and publishes MQTT to the Pi across the
gateway's own access point; the Pi carries it home inside Tailscale. Nothing at
home is exposed, which matters because the gateway cannot do TLS and the home
connection is behind carrier-grade NAT.

**The work splits in two, and the split is the point:** everything that can be
done without knowing the client's network is done at the bench, so the site
visit is one command run from a phone.

## One-time, on the laptop

```sh
git clone git@github.com:nolfonzo/SensorSentinel.git
cd SensorSentinel
cp sensorsentinel.env.example sensorsentinel.env
chmod 600 sensorsentinel.env
$EDITOR sensorsentinel.env          # passwords, and TS_AUTHKEY if you want it unattended
```

Set `AP_PASS`, `PI_PASS` and `DEPLOY_USER` deliberately. `AP_PASS` is
fleet-wide when set, and `PI_PASS` is the login you will need standing on a
dock months from now — there is no way to look it up from a deployed unit.
Leaving `AP_PASS` empty generates a different AP password per pair instead,
which is more secure and much less convenient.

You also need `sshpass`, but **only for the first contact with a factory
gateway** — provisioning installs your SSH key, so everything afterwards uses
key auth.

```sh
brew install hudochenkov/sshpass/sshpass   # macOS (not in homebrew core)
sudo apt install sshpass                   # Linux
```

## Building a pair, at the bench

1. **Flash the SD card** in Raspberry Pi Imager: your bench WiFi, SSH enabled
   with your public key, and — importantly — **set the hostname to
   `pi0-<site>`**.

   That hostname is the only per-unit name anyone types. The gateway derives
   its own name from it (`pi0-north` → `gateway-north`), so the SSID you look
   for at the site and the address you ssh to both follow from this one field.
   Flash it as `raspberrypi` and the gateway keeps its factory `HT-M7603-XXXX`
   name instead.

2. **Power the gateway** — the **5 V barrel connector**. The USB-C port does
   not power it, whatever it looks like.

3. **Join the gateway's own WiFi** from the laptop: `HT-M7603-XXXX`, password
   `heltec.org` on a factory unit. Then point it at your bench network:

   ```sh
   ./gateway/provision-gateway.sh --bench
   ```

4. **Rejoin your own WiFi** and power the Pi. Both are now on one network.

5. **Provision the pair:**

   ```sh
   ./provision-pair.sh --bench
   ```

   It finds both devices, names the gateway from the Pi's hostname, sets the
   radio to AU915, creates the deployment account, binds the Pi to the
   gateway's AP, installs the watchdog, brings up the bridge home, and reboots
   the Pi to prove it comes back by itself.

   With several pairs on the bench, power up **one gateway at a time** — the Pi
   is pinned to a BSSID, but discovery still has to tell them apart.

6. **Confirm a reading arrives at home.** The bench is the only place where a
   failure is cheap.

### What a provisioned unit carries

| | |
|---|---|
| Gateway AP | `gateway-<site>`, password `AP_PASS` |
| Gateway address | `192.168.8.1`, on its own AP, on every unit |
| Pi address | `192.168.8.2`, static, BSSID-pinned to that gateway |
| Login | `DEPLOY_USER` (default `sensorsentinel`) with `PI_PASS` |
| In the home dir | a `set-site-wifi` symlink, so `ls` answers "what was it called" |

The deployment account exists so units carry no personal login. Provisioning
creates it if the flash did not, and verifies it by logging in with the
password rather than trusting that setting it worked.

## At the site

**Phone only — no laptop.** Everything here runs over the gateway's own access
point from an SSH client on a phone (Termius). The laptop sections above exist
so that this one does not need one.

7. **Power both.** The Pi finds its gateway by itself — pinned to that
   gateway's BSSID, it cannot associate with another.

8. **Join `gateway-<site>` on your phone**, using `AP_PASS`.

9. **SSH to the Pi** with Termius or any SSH client:

   ```sh
   ssh sensorsentinel@pi0-<site>.local     # or 192.168.8.2
   ```

   Keep the `.local` — iOS resolves it over Bonjour, a bare hostname often
   will not. `192.168.8.2` always works and needs no name resolution at all.

10. **Find their WiFi, then set it:**

    ```sh
    set-site-wifi --scan                      # what the GATEWAY's radio can see
    set-site-wifi "Their SSID" "password"
    ```

    `--scan` is why you do not need the client's SSID in advance. It surveys
    from the gateway's aerial, not your phone's — the only opinion that
    matters, since your phone may see networks the gateway cannot.

    **Quote both arguments.** Site names contain spaces and apostrophes
    constantly.

11. **Wait for `Looks good.`** then confirm a reading arrives at home before
    you leave. The script checks association, address, forwarder and internet,
    but it cannot see your server — only you can close that loop.

### Expect the connection to drop at step 10

The gateway has **one radio**, not two. Its access point and its uplink are
virtual interfaces sharing a single MT7628AN, so they must be on the same
channel — when the uplink associates to the client's network, the AP follows it
there, and everything joined to the AP drops for around thirty seconds.

Your phone and your SSH session both die. That is expected, not a failure. The
script backgrounds the radio reload and waits precisely because the session
issuing the command cannot survive it. Let it finish, rejoin, and read the
result. It is safe to re-run with corrected details.

## Why the addresses never change

The gateway is **always `192.168.8.1`** on its own access point, on every unit,
so joining that AP is all you need to reach one. The Pi takes a static
**`192.168.8.2`**, because the gateway can only be given an IP for its broker,
never a hostname. Both are defaults in the env file.

## Recovering a Pi that will not join

Plug a USB cable from the laptop into the Pi's **data** micro-USB port (marked
`USB`, not `PWR`) — one cable carries power and network. It appears as a
network interface and you can `ssh sensorsentinel@pi0-<site>.local` with no
WiFi at all. Worth knowing before you need it.

This matters more than it looks. Once a Pi is bound to a gateway's AP, that AP
is its only network, and a Pi Zero has one radio. If it will not join, there is
no second path in short of this cable or a reflash — which is why provisioning
refuses to bind a Pi that is not already reachable over Tailscale.
