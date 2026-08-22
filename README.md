# SensorSentinel

Boat and jetty monitoring over LoRa. A sensor node on the boat reads its battery
and switches every 30 seconds and sends them by radio to a gateway on the jetty;
the gateway passes them to a server at home, which compares them against
thresholds and tells the owner when something changes.

No wifi and no SIM card on the boat, and nothing at home exposed to the internet.

```
  boat                    jetty                         home
  ────                    ─────                         ────

  Heltec node       LoRa    HT-M7603 gateway   MQTT    Pi Zero 2 W
  ├ battery monitor ──────► (AU915, 8 channels) ─────► (bridges up)
  ├ bilge float             frequency hopping              │
  └ 18650, months                                          │ Tailscale
    per charge                                             ▼
                                                     pi5 at home
                                                     ├ Mosquitto
                                                     ├ Node-RED  ── alerts ──►  Telegram
                                                     ├ Postgres                 and/or email
                                                     └ NocoDB / Grafana
```

## What it watches

Each device carries several sensors, and which ones is up to the owner:

- **Battery voltage** via INA219/SEN0291 on the I²C bus — up to four per node
- **Switches** such as a bilge float — four inputs comfortably, six available
- **Its own cell**, and how well its radio is being heard (RSSI/SNR)

Alerts fire on **changes only**: when a reading crosses a limit, and again when
it returns to normal. Silence is the normal state — which is why a device that
stops reporting raises an alert of its own. A sensor that has gone quiet
otherwise looks identical to one saying all is well.

## Telegram

Owners can opt into a Telegram bot for status on demand, live feeds while
standing on the dock, and sensor configuration — or stay on email, which needs
nothing of them.

Pairing is self-service: every alert email carries a one-tap deep link bound to
that owner, so nobody edits a config file or restarts anything to add a person.
A device can also be claimed from its own screen — press the button, read the six
characters, send `/claim 676358`. That works from a chat the bot has never seen.

See **[docs/TELEGRAM.md](docs/TELEGRAM.md)** for the command set, or
[the printable one-pager](docs/telegram-commands.pdf).

## Hardware

| Part | Notes |
|---|---|
| Node | Heltec WiFi LoRa 32 **V3 / V3.2 / V4** — ESP32-S3 + SX1262 |
| Gateway | Heltec **HT-M7603**, AU915 sub-band 0 |
| Bridge | Raspberry Pi Zero 2 W, joined to the gateway's own access point |
| Server | Raspberry Pi 5 — Docker: Mosquitto, Node-RED, Postgres, NocoDB, Grafana |

Battery and USB charging are on the Heltec module itself; the V4 adds a solar
interface. **[docs/HARDWARE.md](docs/HARDWARE.md)** covers usable pins, packaging
and what is still unresolved — read it before wiring a sensor, because three of
the twelve nominal sensor pins cannot be used.

## Repository

```
src/               node firmware (PlatformIO)
gateway/           gateway provisioning + AU915 profile
pi/                Pi Zero bridge: watchdog, site wifi
docker/            the home server stack
  nodered/data/    flows: alerting, Telegram bot, gateway adapter
  postgres/        schema and migrations
docs/              protocol, hardware, Telegram reference
```

## Getting started

| I want to… | Read |
|---|---|
| Build a gateway + Pi pair | [DEPLOY.md](DEPLOY.md) |
| Understand the wire format | [docs/SPECIFICATION.md](docs/SPECIFICATION.md) |
| Wire a sensor to a node | [docs/HARDWARE.md](docs/HARDWARE.md) |
| Use the bot | [docs/TELEGRAM.md](docs/TELEGRAM.md) |
| See what is deployed | [INVENTORY.md](INVENTORY.md) |

Secrets live in `sensorsentinel.env` and `docker/.env`, both gitignored. Copy the
`.example` files beside them.

## Status

Working in service on two boats. Known gaps are documented rather than hidden —
the node's own battery reading is unvalidated, so its low-battery alert has never
been able to fire; see [docs/HARDWARE.md](docs/HARDWARE.md).
