# Deploying a gateway + Pi pair

A pair is one Heltec HT-M7603 LoRa gateway and one Raspberry Pi Zero 2 W. The
gateway hears the boat node over LoRa and publishes MQTT to the Pi across the
local network; the Pi carries it home inside Tailscale. Nothing at home is
exposed, which matters because the gateway cannot do TLS and the home
connection is behind carrier-grade NAT.

## One-time, on the laptop you take to sites

```sh
git clone git@github.com:nolfonzo/SensorSentinel.git
cd SensorSentinel
cp sensorsentinel.env.example sensorsentinel.env
chmod 600 sensorsentinel.env
$EDITOR sensorsentinel.env          # passwords, and TS_AUTHKEY if you want it unattended
```

You also need `sshpass`, but **only for the first contact with a factory
gateway** — provisioning installs your SSH key, so everything afterwards uses
key auth.

```sh
brew install hudochenkov/sshpass/sshpass   # macOS (not in homebrew core)
sudo apt install sshpass                   # Linux
```

## Building a pair, at the bench

1. **Flash the SD card** in Raspberry Pi Imager: your home WiFi, SSH enabled
   with your public key, and a hostname. Put it in the Pi.

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
   ./provision-pair.sh
   ```

   It finds both devices, reads the gateway's MAC-derived AP name, sets the
   radio to AU915, binds the Pi to that gateway, installs the watchdog, and
   brings up the bridge home.

6. **Reboot the Pi** (activates USB gadget rescue mode) and confirm a reading
   arrives at home.

## At the site

7. **Power both.** The Pi finds its gateway by itself — it is pinned to that
   gateway's BSSID and cannot associate with another.

8. **Join `HT-M7603-XXXX`** using your `AP_PASS`, and give the gateway the
   site's WiFi:

   ```sh
   ./gateway/provision-gateway.sh --wifi-ssid "Their WiFi" --wifi-pass 'xxx'
   ```

9. **Confirm a reading arrives at home**, then leave.

If you can get the site's WiFi details beforehand, put them in `SITE_SSID` /
`SITE_PASS` and run step 8 at the bench instead. The visit is then steps 7
and 9.

## Why the addresses never change

The gateway is **always `192.168.8.1`** on its own access point, on every unit,
so joining that AP is all you need to reach one. The Pi takes a static
**`192.168.8.2`** on that network, because the gateway can only be given an IP
for its broker, never a hostname. Both are defaults in the env file.

## Recovering a Pi that will not join

Plug a USB cable from the laptop into the Pi's **data** micro-USB port (marked
`USB`, not `PWR`) — one cable carries power and network. It appears as a
network interface and you can `ssh nolfonzo@<hostname>.local` with no WiFi at
all. Worth knowing before you need it.
