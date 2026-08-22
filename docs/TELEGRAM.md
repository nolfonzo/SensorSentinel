# Telegram

An optional second channel. Email needs nothing of anyone and stays the default;
Telegram adds status on demand, live feeds, and sensor configuration from a phone.

A [printable one-page reference](telegram-commands.pdf) is in this directory.

## Pairing

Self-service, with no admin step. Every alert email carries a one-tap deep link
bound to that owner:

```
https://t.me/<bot>?start=<token>
```

The mailbox is the proof of identity — it is the address already on file, so
whoever can read it is that owner. Tapping the link opens the chat, and the bot
asks where alerts should go. The same token works typed, as `/pair <token>`, for
mail clients that mangle links.

Nothing per-user is ever stored in `.env`; only the bot token is. Chat IDs live
in the database, which is why adding a person needs no config edit and no
restart.

## Commands

### Checking on things

| Command | |
|---|---|
| `/status [device]` | Last reading from each device, with signal and age |
| `/sensors [device]` | Every channel and what it reads — **named or not** |
| `/info` | What you own and how it is configured |
| `/help` | The command list |

Everything is scoped to what you own. With one device, its name can be left out
of any command.

### Watching live

| Command | |
|---|---|
| `/watch [device] [mins]` | Every reading as it arrives, ~1 per 30 s |
| `/unwatch [device]` | Stop feeds — all, or one |

Feeds expire after 30 minutes by default, then stop by themselves. **Stop** on a
feed stops that device; **Another 30m** resets to 30 from now rather than adding.

### Devices

| Command | |
|---|---|
| `/claim <code> [name]` | Take on a device, and name it |
| `/rename <code> <name>` | Give it a proper name |
| `/release <device>` | Hand it to someone else |

The code is the last six hex digits of the MAC, shown on the device's screen when
**PRG** is pressed. It is derived in SQL as
`lpad(to_hex(node_id & 16777215), 6, '0')`, so the screen and the database cannot
disagree.

**Claiming works from a chat the bot has never seen** — an unpaired sender with a
valid code becomes an owner on the spot, with no email and no invitation. But a
device that already has an owner **cannot** be claimed; they must `/release` it
first, so possession of a code can never take a boat off someone.

### Setting up sensors

| Command | |
|---|---|
| `/sensorname <ch> <name>` | Name a channel so it appears elsewhere |
| `/sensoralert <Dn> <when>` | When a switch matters: `open`, `closed`, `change`, `off` |
| `/limits <ch> <low> [high]` | Alert outside a range — `off` to stop |

Channels are `bus0–bus7` (I²C), `A0–A3` (analog), `D0–D7` (switches). See
[HARDWARE.md](HARDWARE.md) — not all of them are usable.

**Naming is not arming.** A named channel appears in `/status` and stays silent
until given a trigger or a limit. `/limits` echoes the live reading back and says
so if what you set is already outside the range — the check that catches a limit
entered in volts against a reading in millivolts.

`/sensoralert` states both directions back: *"will ALERT when it closes, and
RESOLVE when it opens again"*. Half the value of a transition alert is knowing
which way round it is.

### Where alerts go

| Command | |
|---|---|
| `/alerts` | Telegram, email, or both |
| `/email on\|off` | Turn email on or off |
| `/mute [2h]` | Silence everything, every channel |
| `/unmute` | Start again |

`/email off` is **refused** while email is the only way to reach you. Turning off
the only channel is not a preference, it is a silent alarm.

A mute with a duration un-mutes itself; without one it lasts until told
otherwise. A muted owner hears nothing at all — **including a device going
offline**.

## Reading a switch

Inputs are `INPUT_PULLUP`, so they read **1 when open** and **0 when closed** to
ground. An unconnected input therefore looks exactly like a switch sitting open.

To find which input something is wired to: toggle it, run `/sensors` again, and
see which digit moved.

## Limits

Readings travel **one way**, from the boat to shore. The bot cannot ask a device
anything, so every view is the most recent message it sent — usually under a
minute old, and older if it has gone quiet. That is why the age is printed beside
the name.

## Setup

One bot serves everyone; users pair to it. `TELEGRAM_BOT_TOKEN` goes in
`docker/.env` — and because function nodes read it from the process environment,
**the container must be recreated, not just the flows redeployed**.

Leave it empty and the bot idles harmlessly; email is unaffected.
