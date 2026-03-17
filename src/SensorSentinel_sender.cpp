/**
 * @file SensorSentinel_sender.cpp
 * @brief Deep-sleep sensor sender — wake, transmit, sleep
 *
 * On each wake cycle: read sensors, transmit a sensor packet, and go back
 * to deep sleep. GNSS packets are sent every 3rd wake cycle (when GNSS is
 * defined). The send interval is configurable via the diagnostic web page
 * and persisted in NVS.
 *
 * Cold boot / button wake: full startup with display info, diagnostic
 * check, and LED mode indicator.
 * Timer wake: minimal path — transmit and sleep immediately.
 */

#include "heltec_unofficial_revised.h"
#include "SensorSentinel_packet_helper.h"
#include "SensorSentinel_diag.h"

#ifndef NO_RADIOLIB
#include "SensorSentinel_RadioLib_helper.h"
#endif

// RTC-retained state (survives deep sleep, resets on power loss)
RTC_DATA_ATTR uint32_t sensorPacketCounter = 0;
RTC_DATA_ATTR uint32_t gnssPacketCounter = 0;
RTC_DATA_ATTR uint8_t wakeCount = 0;

// Forward declarations
void sendSensorPacket();
void sendGnssPacket();
void flashLedForMode(int interval);

void setup() {
  heltec_setup();

  bool timerWake = heltec_wakeup_was_timer();

  // --- Cold boot or button wake: full startup ---
  if (!timerWake) {
    SensorSentinel_diag_check();

    heltec_clear_display();
    both.println("\nSensorSentinel");
    both.printf("Board: %s\n", heltec_get_board_name());
    both.printf("Battery: %d%% (%.2fV)\n",
               heltec_battery_percent(),
               heltec_vbat());

    int interval = SensorSentinel_diag_get_interval();
    both.printf("Mode: %s (%ds)\n", (interval <= 30) ? "Fast" : "Slow", interval);
    heltec_display_update();

    flashLedForMode(interval);
    delay(2000);

    // Reset wake counter on cold boot
    wakeCount = 0;
  }

  // --- Read interval from NVS ---
  int interval = SensorSentinel_diag_get_interval();

  // --- Always send sensor packet ---
  sendSensorPacket();

  // --- Send GNSS every 3rd wake cycle ---
#ifdef GNSS
  if (wakeCount % 3 == 0) {
    sendGnssPacket();
  }
#endif

  wakeCount++;

  // --- Deep sleep ---
  heltec_deep_sleep(interval);
}

void loop() {
  // Should never execute — deep sleep restarts from setup().
  // Safety net in case deep sleep fails.
  heltec_deep_sleep(SensorSentinel_diag_get_interval());
}

/**
 * Flash LED to indicate current mode on cold boot.
 * 1 flash = fast (30s), 2 flashes = slow (5min)
 */
void flashLedForMode(int interval) {
  int flashes = (interval <= 30) ? 1 : 2;
  for (int i = 0; i < flashes; i++) {
    heltec_led(50);
    delay(200);
    heltec_led(0);
    if (i < flashes - 1) delay(200);
  }
}

/**
 * Create and send a sensor packet
 */
void sendSensorPacket() {
  SensorSentinel_sensor_packet_t packet;
  bool initSuccess = SensorSentinel_init_sensor_packet(&packet, sensorPacketCounter++);

  if (!initSuccess) {
    Serial.printf("ERROR: init sensor pkt fail\n");
    return;
  }

  Serial.println("Sending Pkt: Sensor");
  Serial.printf("Msg: #%u  NodeID: %u  Bat: %u%%\n",
               packet.messageCounter, packet.nodeId, packet.batteryLevel);

  heltec_led(25);

#ifndef NO_RADIOLIB
  int state = radio.transmit((uint8_t*)&packet, sizeof(packet));
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("Sensor packet sent OK");
  } else {
    Serial.printf("ERROR: TX failed: %d\n", state);
  }
#else
  Serial.println("No Radio");
#endif

  heltec_led(0);

  SensorSentinel_print_packet_info(&packet, sizeof(packet));
  Serial.println("---------------------------\n");
}

/**
 * Create and send a GNSS packet
 */
void sendGnssPacket() {
  SensorSentinel_gnss_packet_t packet;
  bool hasFix = SensorSentinel_init_gnss_packet(&packet, gnssPacketCounter);

  Serial.println("Sending Pkt: GNSS");
  Serial.printf("Msg: #%u  NodeID: %u  Bat: %u%%\n",
               packet.messageCounter, packet.nodeId, packet.batteryLevel);

  heltec_led(25);

#ifndef NO_RADIOLIB
  int state = radio.transmit((uint8_t*)&packet, sizeof(packet));
  if (state == RADIOLIB_ERR_NONE) {
    gnssPacketCounter++;
    Serial.println("GNSS packet sent OK");
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
}
