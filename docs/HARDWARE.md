# Hardware

Usable pins, board support, power and packaging — and what is still unresolved.

Read the pin table before wiring anything: **three of the twelve nominal sensor
pins cannot be used**, and one of them is triple-booked.

---

## Usable pins — Heltec V3 / V3.2

The firmware offers `A0–A3` and `D0–D7`, but several collide with functions the
board already uses. Verified against `heltec_unofficial_revised.h` and
`SensorSentinel_pins_helper.h`.

| Channel | GPIO | Status | Conflict |
|---|---|---|---|
| `A0` | 1 | **unusable** | `VBAT_ADC` battery sense **and** I²C SDA |
| `A1` | 2 | **unusable** | I²C SCL |
| `A2` | 3 | **avoid** | strapping pin |
| `A3` | 4 | free | — |
| `D0` | 33 | free | — |
| `D1` | 34 | free | — |
| `D2` | 35 | **unusable** | onboard LED |
| `D3` | 39 | free | — |
| `D4` | 40 | free | float switch on both deployed nodes |
| `D5` | 41 | free | — |
| `D6` | 42 | free | — |
| `D7` | 46 | **avoid** | strapping pin |

**Real availability: 6 digital, 1 analog.**

This is why `/sensors` reports `A0: 0` and `A1: 0` permanently — those pins are
the I²C bus, not free inputs. Six digital covers the four sensors a node needs
with room spare, so the constraint is not scarcity; it is that wiring to a dead
pin looks plausible and silently never works.

> **Live consequence.** "Main Power Supply" on *Nolf big boat* is configured on
> `A1`, which is the I²C clock line. That channel can never read anything. It is
> dormant only because its alert level is `None`. Move it to `A3`, or take supply
> voltage from an INA219 on the bus instead.

### Reserved by the board

`0` PRG · `1` VBAT_ADC · `8–14` LoRa (SX1262) · `17,18` OLED · `21` OLED reset ·
`35` LED · `36` Vext · `37` VBAT_CTRL

### V4 differences

The V4 is the same ESP32-S3 + SX1262 and keeps the V3 form factor and pin layout,
so it is a board define rather than a port.

| | V3 | V4 |
|---|---|---|
| Battery | SH1.25-2 | 1.25×2P |
| Charging | onboard — charge/discharge management, overcharge protection, USB/battery auto-switching | same family |
| Solar | none onboard | onboard interface (1.25×2P) |
| TX power | ~21 dBm | 28±1 dBm |
| **Onboard LED** | GPIO35 → kills `D2` | GPIO35, **or GPIO46 on the R8 variant → kills `D7`** |

That last row matters: the dead digital pin **moves by variant**. Verify against
the board in front of you, not the datasheet.

---

## I²C sensors

Up to **four** INA219/SEN0291 on one bus, at addresses `0x40`, `0x41`, `0x44`,
`0x45`, set by the DIP switches. `identify_sensor()` accepts only those four for
this part; INA226 is recognised across `0x40–0x4F` and ADS1115 across
`0x48–0x4B` if more are ever needed.

Bus pins are **SDA 1, SCL 2**. Discovery results are cached in `RTC_DATA_ATTR`,
so they survive deep sleep *and* a reset — **after swapping a sensor, remove
power rather than pressing reset**, or the stale result persists.

---

## Power

Battery and USB charging are on the Heltec module; nothing external is needed.

**The divider is unvalidated.** With no cell fitted, `VBAT_ADC` sits near the top
of its range and `analogRead(VBAT_ADC) / 238.7` reports about **16 V** — above
what a single 18650 can physically reach. The consequence is not cosmetic: the
*"Transmitter 18650 Battery ≤20%"* alert has never been able to fire, and cannot
while it reads 100%. That is the alert that warns a node is about to go silent.

### Validating it

1. Measure the cell on a multimeter. That is the reference.
2. Fit it and **unplug USB** — on USB the ADC reads the rail.
3. Press **PRG**, read the `batt` line, cross-check `voltage` in the next packet.
4. **Pass:** within ~0.1 V of the meter, and inside 3.0–4.2 V.
5. If off by a constant ratio, the new divisor is `238.7 × (reported ÷ actual)`
   in `heltec_vbat()`.
6. Confirm it **tracks** — run the cell down and check the reading follows.
7. Run several days on battery, then pull the discharge curve from `events`.

**Acceptance is the alert firing**, not the number looking right.

### Reporting the power source

`nodeFlags` in the packet is documented as *"Power source & operating mode"* and
unused — bit 7 is repeater mode, everything else is `0`. Setting a bit for
"running on a cell" costs nothing and lets the server suppress the low-battery
alert when no cell is fitted, instead of leaving a dead alarm that looks armed.

What is cheaply knowable is **whether a cell is fitted** (below ~4.5 V), not
whether USB is connected — that needs a real signal, and is a separate job.

---

## Packaging

**Condensation matters more than spray.** A sealed box in a marine environment
needs a breathable vent or desiccant, not just a good gasket.

- Antenna outside via bulkhead SMA
- Waterproof glands or terminals for sensor wires
- Strain relief, so no solder joint is load-bearing
- Mount well above the waterline

Heltec sell a Solar Kit for the V3 that bundles solar with a waterproof outdoor
enclosure — worth pricing before designing a box, since it may answer both.

### Pi Zero inside the gateway

One unit instead of two, powered from the gateway's 5 V / 2 A rail. Three checks
first:

| Check | Why |
|---|---|
| Current headroom | A Zero 2 W peaks near 1.2 A on a 2 A rail shared with the gateway. Measure both under load. |
| Thermal | A second board in a sealed enclosure, outdoors, in the sun. |
| Rescue path | Keep the USB-gadget route reachable. Sealed inside the gateway, it is the only way in short of reflashing. |

---

## The open-boat-projects board

[LoRa Bootsmonitor](https://open-boat-projects.org/en/lora-bootsmonitor/) by
Norbert Walter is a well-made design, proven in service — and built for a
different problem.

| | That board | Needed here |
|---|---|---|
| Board | Heltec **V2** — ESP32 + SX1276 | V3/V4 — ESP32-S3 + SX1262 |
| Power | 10–32 V boat supply, DC/DC at 5.7 V | Battery, USB charge, solar |
| Charging | none — assumes shore power | central |
| Carries | GPS, VE.Direct, relay, tank senders | none of these |
| Source | Aisler order link + PDF; no gerbers or schematic source | editable, to modify |

**Verdict: use it as a reference, don't adapt the boards.** V2 and V3 differ in
MCU family *and* radio, so GPIO numbering is entirely different — every signal
into the module socket would need cutting and rerouting. And with no schematic
source published, the design cannot be modified, only the physical board. That is
a lot of surgery to reach something that still lacks battery, charging and solar.

Worth borrowing: the 10–32 V input divider for reading boat supply directly; the
through-hole-only, field-repairable philosophy; IP68 with terminal strips; and
the interactive BOM idea.

---

## Open questions

- **Which V4 variant** — R2 or R8 decides whether the dead digital pin is `D2` or `D7`
- **Analog channels** — only `A3` is free. Enough, or read supply voltage over I²C?
- **Solar or swap** — answer from the measured discharge curve, not a guess
- **Gateway power** — what feeds the jetty gateway once the Pi draws from it too
