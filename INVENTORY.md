# Deployed pairs

Appended automatically by `provision-pair.sh`, one row per pair. Identifiers
only - credentials live in `sensorsentinel.env`, which is not in git.

The **BSSID** is the gateway's radio MAC. It identifies a physical box even
after a rename or an address change, which is what you want when several
units are on a bench and none of them are labelled.

| Provisioned | Gateway | Gateway BSSID | Pi | Pi Tailscale | Uplink WiFi | Notes |
|---|---|---|---|---|---|---|
| 2026-08-21 | HT-M7603-13B7 | 40:D6:3C:81:13:B7 | pi0-bench | 100.116.215.30 | Nuevo Extremo | Bench rig. Not on the jetty architecture: the Pi sits on the house WiFi and the gateway publishes to its **DHCP** address (192.168.9.145) rather than to 192.168.8.2 over the gateway's own AP. Works, but breaks silently if that lease changes. |
