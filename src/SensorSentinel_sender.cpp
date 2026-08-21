/**
 * @file SensorSentinel_sender.cpp
 * @brief Sensor sender with optional repeater mode
 *
 * Two modes selected at compile time via REPEATER_MODE:
 *
 * REPEATER_MODE = false  (battery-powered sender):
 *   Wake from deep sleep, transmit sensor packet, sleep again.
 *   GNSS packet sent every 3rd wake. Send interval configurable via
 *   diagnostic web UI and persisted in NVS.
 *
 * REPEATER_MODE = true  (mains-powered repeater):
 *   Stays awake continuously. Listens for packets from other nodes and
 *   re-transmits them (with deduplication to prevent loops). Also sends
 *   its own sensor data on a timer. No deep sleep.
 *
 * Set via platformio.ini build flag: -DREPEATER_MODE=1
 */

#include "heltec_unofficial_revised.h"
#include "SensorSentinel_packet_helper.h"
#include "SensorSentinel_diag.h"
#include "SensorSentinel_i2c_helper.h"

#ifndef NO_RADIOLIB
#include "SensorSentinel_RadioLib_helper.h"
#endif

// ── Mode selection ─────────────────────────────────────────────────────────────
#ifndef REPEATER_MODE
#define REPEATER_MODE false
#endif

// ── RTC state (survives deep sleep, resets on power loss) ─────────────────────
RTC_DATA_ATTR uint32_t sensorPacketCounter = 0;
RTC_DATA_ATTR uint32_t gnssPacketCounter   = 0;
RTC_DATA_ATTR uint8_t  wakeCount           = 0;
RTC_DATA_ATTR static int      _txChannel   = -1; // -1 = hop, 0-7 = locked
RTC_DATA_ATTR static uint32_t _hopState    = 0;  // PRNG state, persists across sleeps
RTC_DATA_ATTR static bool     _rtcSeeded   = false;
// How many I2C sensors were seen on the last wake, for the PRG identity screen.
// A count rather than values: the device does not know what any slot means -
// the labels live server-side - so a raw "slot 0: 10692" is less use than
// /status, which knows it is the boat battery. The count still answers the
// question you have during an install: are the sensors being seen at all.
// Cached because the OLED and the sensors share the Wire instance and the VEXT
// rail, so reading I2C in the button path fights the display for both.
RTC_DATA_ATTR static uint8_t  _lastSensorCount = 0;

// ── Repeater state ─────────────────────────────────────────────────────────────
#if REPEATER_MODE
#define REPEAT_DELAY_MS   200   // Pause before re-transmitting (keep short, radio is deaf during TX)
#define PACKET_CACHE_SIZE  50

struct PacketCacheEntry { uint32_t nodeId; uint32_t messageCounter; };
static PacketCacheEntry _seenPackets[PACKET_CACHE_SIZE];
static uint8_t          _cacheIndex = 0;
static unsigned long    _lastOwnSensorTx = 0;
static uint32_t         _packetsRepeated = 0;
static unsigned long    _sensorIntervalMs = 60000; // loaded from NVS in setup()
static unsigned long    _nextSensorGapMs  = 60000; // _sensorIntervalMs + jitter
#endif

// ── TX channel selection ───────────────────────────────────────────────────────
static void initTxChannel(bool isColdBoot) {
  _txChannel = SensorSentinel_diag_get_channel();
  if (!_rtcSeeded || isColdBoot) {
    _hopState = (uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFFULL);
    if (_hopState == 0) _hopState = 1;   // a zero seed would never advance
    _rtcSeeded = true;
  }
}

#ifndef NO_RADIOLIB
static void applyTxChannel() {
  int idx;
  if (_txChannel >= 0 && _txChannel < AU915_SB0_COUNT) {
    idx = _txChannel;
  } else {
    // Advance PRNG state on each transmission
    _hopState = _hopState * 1664525u + 1013904223u;   // Numerical Recipes LCG
    idx = (_hopState >> 16) & (AU915_SB0_COUNT - 1);
  }
  radio.setFrequency(AU915_SB0[idx]);
  Serial.printf("[LoRa] Channel %d: %.1f MHz\n", idx, AU915_SB0[idx]);
}

// setFrequency() is sticky, so after a hopped transmission the radio is still
// tuned to whichever channel it just used. A repeater must go back to its
// listening channel or it silently starts monitoring the wrong one.
static void restoreRxChannel() {
#if REPEATER_MODE
  radio.setFrequency(HELTEC_LORA_FREQ);
#endif
}
#endif

// Fine-grained millisecond jitter: ±3000 ms to prevent synchronous collisions
static unsigned long jitteredMs(unsigned long baseMs) {
  long jitter = (long)(esp_random() % 6001UL) - 3000L;
  long result = (long)baseMs + jitter;
  return (result < 1000L) ? 1000UL : (unsigned long)result;
}

// ── Sleep, with PRG armed as a wake source ──────────────────────────────────
// The library's heltec_deep_sleep() arms this, but the sender has always gone
// to sleep through the ESP APIs directly and so never enabled a button wake -
// which is why pressing PRG did nothing at all.
//
// BUTTON is GPIO0, an RTC GPIO on the ESP32-S3, so it can wake us from deep
// sleep. (The digital sensor slots cannot, which is why wake-on-float is not
// possible but wake-on-PRG is.)
static void senderSleep() {
  unsigned long sleepMs = jitteredMs((unsigned long)SensorSentinel_diag_get_interval() * 1000UL);
  Serial.printf("[Power] Deep sleeping for %lu ms...\n\n", sleepMs);

  // ext0 is level triggered: going to sleep while the button is still held
  // would wake us immediately, in a loop. Wait for the release first.
  pinMode(BUTTON, INPUT_PULLUP);
  while (digitalRead(BUTTON) == LOW) delay(10);
  esp_sleep_enable_ext0_wakeup(BUTTON, LOW);

  esp_sleep_enable_timer_wakeup((uint64_t)sleepMs * 1000ULL);
  esp_deep_sleep_start();
}

// ── PRG identity screen ─────────────────────────────────────────────────────
// Shows the two things worth knowing while standing next to the boat: which
// device this is, and how the batteries are doing. The code shown is what gets
// typed into the bot to claim the device.
static void showIdentityScreen() {
  // The low 24 bits of the node id are the last three MAC bytes. Derived from
  // the node id rather than read separately, so the six characters on the
  // screen always agree with what the server sees in the packet.
  uint32_t nodeId = SensorSentinel_generate_node_id();
  char id6[7];
  snprintf(id6, sizeof(id6), "%06X", (unsigned)(nodeId & 0xFFFFFFu));

  float vb = heltec_vbat();

  // No I2C here, deliberately. The OLED and the sensor bus share one Wire
  // instance on different pins (display 17/18, sensors 1/2) and the same VEXT
  // rail, so reading sensors here repoints the bus and power-cycles the panel -
  // the serial output looks perfect and the screen stays dark. Use the value
  // cached on the last normal wake instead.

  heltec_clear_display();
  both.printf("ID  %s\n\n", id6);

  // The device's own battery is the headline figure - it is what you want to
  // know standing next to the boat, and it is the one nothing else reports.
  // Always show the actual number. Above ~4.5V there is no cell fitted and the
  // ADC is sitting near the USB rail, so annotate it rather than hide it:
  // suppressing the value loses the reading you came to see.
  both.printf("batt %.2fV %d%%\n", vb, heltec_battery_percent(vb));
  if (vb > 4.5f) both.printf("  (on USB, no cell)\n");

  both.printf("sensors %d\n", _lastSensorCount);

  heltec_display_update();
  Serial.printf("[Ident] %s  vbat %.2f  sensors %u\n", id6, vb, _lastSensorCount);

  delay(10000);
}

// ── Forward declarations ───────────────────────────────────────────────────────
void sendSensorPacket();
void sendGnssPacket();
void flashLedForMode(int interval);

#if REPEATER_MODE
void onPacketReceived(uint8_t *data, size_t length, float rssi, float snr);
bool alreadySeen(uint32_t nodeId, uint32_t messageCounter);
void markSeen(uint32_t nodeId, uint32_t messageCounter);
void repeatPacket(uint8_t *data, size_t length);
#endif

// ── setup() ───────────────────────────────────────────────────────────────────
void setup() {
  heltec_setup();

#if REPEATER_MODE
  // ── Repeater mode: full startup, stay awake ──────────────────────────────
  SensorSentinel_diag_check();
  initTxChannel(true);

  heltec_clear_display();
  both.println("\nSensorSentinel");
  both.printf("Board: %s\n", heltec_get_board_name());
  both.printf("Mode: Repeater\n");
  if (_txChannel < 0) both.printf("CH: HOP\n");
  else                both.printf("CH: %.1f LOCK\n", AU915_SB0[_txChannel]);
  both.printf("Battery: %d%% (%.2fV)\n", heltec_battery_percent(), heltec_vbat());
  heltec_display_update();
  delay(2000);

  // Initialize I2C sensors
  SensorSentinel_i2c_auto_discover(false);

  memset(_seenPackets, 0, sizeof(_seenPackets));
  _sensorIntervalMs = (unsigned long)SensorSentinel_diag_get_sensor_interval() * 1000UL;
  _nextSensorGapMs  = jitteredMs(_sensorIntervalMs);

#ifndef NO_RADIOLIB
  if (SensorSentinel_subscribe(NULL, onPacketReceived)) {
    Serial.println("Repeater: listening for packets");
  } else {
    Serial.println("Repeater: subscribe failed");
  }
#endif

  // Fall through to loop() — no deep sleep

#else
  // ── Sender (deep sleep) mode ──────────────────────────────────────────────
  // A PRG press while asleep asks the board to identify itself; it is not a
  // request to transmit. Show the screen and go straight back down, without
  // touching the radio or burning a message counter.
  if (heltec_wakeup_was_button()) {
    showIdentityScreen();
    senderSleep();
  }

  bool timerWake = heltec_wakeup_was_timer();
  initTxChannel(!timerWake);

  if (!timerWake) {
    SensorSentinel_diag_check();

    // Cold boot display banner & full I2C auto-discovery
    heltec_clear_display();
    both.println("\nSensorSentinel");
    both.printf("Board: %s\n", heltec_get_board_name());
    both.printf("Battery: %d%% (%.2fV)\n", heltec_battery_percent(), heltec_vbat());

    int interval = SensorSentinel_diag_get_interval();
    both.printf("Interval: %ds\n", interval);
    if (_txChannel < 0) both.printf("CH: 8-CH HOP\n");
    else                both.printf("CH: %.1f LOCK\n", AU915_SB0[_txChannel]);

    uint8_t i2cDevs = SensorSentinel_i2c_auto_discover(true);
    both.printf("I2C Sensors: %d found\n", i2cDevs);
    heltec_display_update();

    SensorSentinel_i2c_print_discovered();
    flashLedForMode(interval);
    delay(2000);

    wakeCount = 0;
  } else {
    // Fast path on 30s timer wake: ensure sensor power rail is on with POR settling
    SensorSentinel_i2c_power_rail(true);
  }

  sendSensorPacket();

  // Stash the sensor count for the PRG screen. Free here: the bus is already
  // powered and pointed at the sensors, and the display is not in use.
  _lastSensorCount = SensorSentinel_i2c_auto_discover(false);

#ifdef GNSS
  if (wakeCount % 3 == 0) {
    sendGnssPacket();
  }
#endif

  wakeCount++;

  // Put peripherals to sleep to minimize quiescent current
#ifndef NO_RADIOLIB
  radio.sleep(false);
#endif
  SensorSentinel_i2c_power_rail(false);

  senderSleep();
#endif
}

// ── loop() ────────────────────────────────────────────────────────────────────
void loop() {
#if REPEATER_MODE
  heltec_loop();

#ifndef NO_RADIOLIB
  SensorSentinel_process_packets();
#endif

  // Send own sensor data on a timer, re-jittered after every send so this node
  // cannot stay in lockstep with another one.
  if (millis() - _lastOwnSensorTx >= _nextSensorGapMs) {
    sendSensorPacket();
    _lastOwnSensorTx = millis();
    _nextSensorGapMs = jitteredMs(_sensorIntervalMs);
  }

#else
  // Sender mode: loop() should never run (deep sleep restarts from setup).
  senderSleep();
#endif
}

// ── Shared helpers ─────────────────────────────────────────────────────────────
void flashLedForMode(int interval) {
  int flashes = (interval <= 30) ? 1 : 2;
  for (int i = 0; i < flashes; i++) {
    heltec_led(50);
    delay(200);
    heltec_led(0);
    if (i < flashes - 1) delay(200);
  }
}

void sendSensorPacket() {
  SensorSentinel_sensor_packet_t packet;
  bool initSuccess = SensorSentinel_init_sensor_packet(&packet, sensorPacketCounter);

  if (!initSuccess) {
    Serial.println("ERROR: init sensor pkt fail");
    return;
  }

  Serial.printf("Sending Sensor #%u  NodeID: %u  Bat: %u%%\n",
                packet.messageCounter, packet.nodeId, packet.batteryLevel);

  heltec_led(25);

#ifndef NO_RADIOLIB
  applyTxChannel();
  int state = radio.transmit((uint8_t*)&packet, sizeof(packet));
  restoreRxChannel();
  if (state == RADIOLIB_ERR_NONE) {
    sensorPacketCounter++;
    Serial.println("Sensor TX OK");
  } else {
    Serial.printf("ERROR: TX failed: %d\n", state);
  }
#else
  sensorPacketCounter++;
  Serial.println("No Radio");
#endif

  heltec_led(0);
  SensorSentinel_print_packet_info(&packet, sizeof(packet));
  Serial.println("---------------------------\n");

#if REPEATER_MODE
  // Resume listening after TX
  SensorSentinel_subscribe(NULL, onPacketReceived);
#endif
}

void sendGnssPacket() {
  SensorSentinel_gnss_packet_t packet;
  SensorSentinel_init_gnss_packet(&packet, gnssPacketCounter);

  Serial.printf("Sending GNSS #%u  NodeID: %u  Bat: %u%%\n",
                packet.messageCounter, packet.nodeId, packet.batteryLevel);

  heltec_led(25);

#ifndef NO_RADIOLIB
  applyTxChannel();
  int state = radio.transmit((uint8_t*)&packet, sizeof(packet));
  restoreRxChannel();
  if (state == RADIOLIB_ERR_NONE) {
    gnssPacketCounter++;
    Serial.println("GNSS TX OK");
  } else {
    Serial.printf("ERROR: TX failed: %d\n", state);
  }
#else
  gnssPacketCounter++;
  Serial.println("No Radio");
#endif

  heltec_led(0);
  SensorSentinel_print_packet_info(&packet, sizeof(packet));
  Serial.println("---------------------------\n");

#if REPEATER_MODE
  SensorSentinel_subscribe(NULL, onPacketReceived);
#endif
}

// ── Repeater-only functions ────────────────────────────────────────────────────
#if REPEATER_MODE

bool alreadySeen(uint32_t nodeId, uint32_t messageCounter) {
  for (int i = 0; i < PACKET_CACHE_SIZE; i++) {
    if (_seenPackets[i].nodeId == nodeId &&
        _seenPackets[i].messageCounter == messageCounter) {
      return true;
    }
  }
  return false;
}

void markSeen(uint32_t nodeId, uint32_t messageCounter) {
  _seenPackets[_cacheIndex] = { nodeId, messageCounter };
  _cacheIndex = (_cacheIndex + 1) % PACKET_CACHE_SIZE;
}

void repeatPacket(uint8_t *data, size_t length) {
  // NOTE: Single-radio limitation — the radio is deaf during this entire TX window
  // (REPEAT_DELAY_MS + transmission time, typically 200ms–2s depending on SF).
  // Any packets arriving from other nodes during this window are lost.
  // A two-radio design would solve this, but is not supported by these boards.
  delay(REPEAT_DELAY_MS);
  heltec_led(50);
  applyTxChannel();
  int state = radio.transmit(data, length);
  restoreRxChannel();
  heltec_led(0);

  if (state == RADIOLIB_ERR_NONE) {
    _packetsRepeated++;
    Serial.printf("Repeat TX OK (total: %u)\n", _packetsRepeated);
  } else {
    Serial.printf("Repeat TX failed: %d\n", state);
  }

  // Resume listening after TX
  SensorSentinel_subscribe(NULL, onPacketReceived);
}

void onPacketReceived(uint8_t *data, size_t length, float rssi, float snr) {
  // NOTE: If two packets arrive before this callback completes, the radio's
  // single receive buffer means only one will be processed. The interrupt flag
  // (_packetReceived) is a single bool — there is no queue. High-traffic
  // environments will see packet loss at the repeater. This is a hardware
  // constraint of single-radio LoRa nodes.
  if (!SensorSentinel_validate_packet(data, length)) {
    Serial.println("Repeater: invalid packet, skipping");
    return;
  }

  uint32_t nodeId     = SensorSentinel_extract_node_id_from_packet(data);
  uint32_t msgCounter = SensorSentinel_get_message_counter_from_packet(data);

  // Don't repeat our own packets
  if (nodeId == SensorSentinel_generate_node_id()) {
    return;
  }

  if (alreadySeen(nodeId, msgCounter)) {
    Serial.printf("Repeater: duplicate from %u #%u, skipping\n", nodeId, msgCounter);
    return;
  }

  markSeen(nodeId, msgCounter);

  Serial.printf("Repeater: forwarding from %u #%u (RSSI %.1f)\n", nodeId, msgCounter, rssi);

  heltec_clear_display();
  both.printf("Repeat node %u\n", nodeId);
  both.printf("Msg #%u\n", msgCounter);
  both.printf("RSSI: %.1f dB\n", rssi);
  both.printf("Total fwd: %u\n", _packetsRepeated + 1);
  heltec_display_update();

  repeatPacket(data, length);
}

#endif // REPEATER_MODE
