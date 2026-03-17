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
#endif

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

  heltec_clear_display();
  both.println("\nSensorSentinel");
  both.printf("Board: %s\n", heltec_get_board_name());
  both.printf("Mode: Repeater\n");
  both.printf("Battery: %d%% (%.2fV)\n", heltec_battery_percent(), heltec_vbat());
  heltec_display_update();
  delay(2000);

  memset(_seenPackets, 0, sizeof(_seenPackets));
  _sensorIntervalMs = (unsigned long)SensorSentinel_diag_get_sensor_interval() * 1000UL;

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
  bool timerWake = heltec_wakeup_was_timer();

  if (!timerWake) {
    SensorSentinel_diag_check();

    heltec_clear_display();
    both.println("\nSensorSentinel");
    both.printf("Board: %s\n", heltec_get_board_name());
    both.printf("Battery: %d%% (%.2fV)\n", heltec_battery_percent(), heltec_vbat());

    int interval = SensorSentinel_diag_get_interval();
    both.printf("Mode: %s (%ds)\n", (interval <= 30) ? "Fast" : "Slow", interval);
    heltec_display_update();

    flashLedForMode(interval);
    delay(2000);

    wakeCount = 0;
  }

  sendSensorPacket();

#ifdef GNSS
  if (wakeCount % 3 == 0) {
    sendGnssPacket();
  }
#endif

  wakeCount++;
  heltec_deep_sleep(SensorSentinel_diag_get_interval());
#endif
}

// ── loop() ────────────────────────────────────────────────────────────────────
void loop() {
#if REPEATER_MODE
  heltec_loop();

#ifndef NO_RADIOLIB
  SensorSentinel_process_packets();
#endif

  // Send own sensor data on a timer
  if (millis() - _lastOwnSensorTx >= _sensorIntervalMs) {
    sendSensorPacket();
    _lastOwnSensorTx = millis();
  }

#else
  // Sender mode: loop() should never run (deep sleep restarts from setup).
  heltec_deep_sleep(SensorSentinel_diag_get_interval());
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
  int state = radio.transmit((uint8_t*)&packet, sizeof(packet));
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
  int state = radio.transmit((uint8_t*)&packet, sizeof(packet));
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
  int state = radio.transmit(data, length);
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
