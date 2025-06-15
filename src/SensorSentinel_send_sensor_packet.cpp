/**
 * @file SensorSentinel_send_sensor_packet.cpp
 * @brief Periodic sensor and GNSS packet sender
 * 
 * Sends sensor packets every 30 seconds and GNSS packets every 90 seconds
 * using the heltec_unofficial_revised and SensorSentinel_packeti_helper libraries.
 */

#include "heltec_unofficial_revised.h"
#include "SensorSentinel_packet_helper.h"

// Configuration
// #define SENSOR_INTERVAL 30000  // Sensor send interval in milliseconds 
// #define GNSS_INTERVAL   90000  // GNSS send interval in milliseconds 

// Forward declarations
void sendSensorPacket();
void sendGnssPacket();

// Global variables
uint32_t sensorPacketCounter = 0;
uint32_t gnssPacketCounter = 0;
unsigned long lastSensorSendTime = 0;
unsigned long lastGnssSendTime = 0;

void setup() {
  // Initialize the Heltec board
  heltec_setup();

  // Show startup message
  heltec_clear_display();

  both.println("\nSensor Packet Sender");
  both.printf("Board: %s\n", heltec_get_board_name());
  both.printf("Battery: %d%% (%.2fV)\n", 
             heltec_battery_percent(), 
             heltec_vbat());
  heltec_display_update();
  delay(2000);
  
  // Initialize timers with offset to avoid sending both packet types at once
  lastSensorSendTime = millis() - (LORA_PUB_SENSOR_INTERVAL - 5000);  // Send first sensor packet in 5 seconds
  lastGnssSendTime = millis() - (LORA_PUB_GNSS_INTERVAL - 15000);     // Send first GNSS packet in 15 seconds
  
  
  // Show send schedule
  heltec_clear_display();
  both.println("\nSend Schedule");
  both.println("\nIntervals:");
  both.printf("Sensor data: %dsec\n", LORA_PUB_SENSOR_INTERVAL/1000);
  both.printf("GNSS data: %dsec\n", LORA_PUB_GNSS_INTERVAL/1000);
  both.println("\nTransmitting...");
  heltec_display_update();
  
}

void loop() {
  // Handle system tasks
  heltec_loop();
  
  // Check if it's time to send a sensor packet
  if (millis() - lastSensorSendTime >= LORA_PUB_SENSOR_INTERVAL) {
    sendSensorPacket();
    lastSensorSendTime = millis();
  }
 
  #ifdef GNSS  
    // Check if it's time to send a GNSS packet
    if (millis() - lastGnssSendTime >= LORA_PUB_GNSS_INTERVAL) {
      sendGnssPacket();
      lastGnssSendTime = millis();
    }
  #endif
  
  // Use heltec_delay to ensure power button functionality works
  //heltec_delay(10);
}

/**
 * Create and send a sensor packet
 */
void sendSensorPacket() {
  heltec_clear_display();

  // Create and initialize the sensor packet
  SensorSentinel_sensor_packet_t packet;
  bool initSuccess = SensorSentinel_init_sensor_packet(&packet, sensorPacketCounter++);

  if (!initSuccess) {
    both.printf("ERROR: init pkt fail: %d\n", initSuccess);
    heltec_display_update();
    delay(2000);  // Let the error message display
    return;
  } 

  // Show basic info on display
  both.println("\nSending Pkt: Sensor");
  both.printf("Packet #%u\n", packet.messageCounter);
  both.printf("NodeID: %u\n", packet.nodeId);
  both.printf("Battery: %u%% (%.2fV)\n", 
             packet.batteryLevel, 
             packet.batteryVoltage / 1000.0f);
  // Turn on LED during transmission
  heltec_led(25);
  
  // Send the packet using RadioLib
  int state = radio.transmit((uint8_t*)&packet, sizeof(packet));
  
  if (state == RADIOLIB_ERR_NONE) {
    both.println("Sensor packet sent OK");
  } else {
    both.printf("ERROR: TX failed: %d\n", state);
    heltec_display_update();
    delay(2000);  // Let the error message display
    return;
  }
  
  // Turn off LED
  heltec_led(0);
  
  // Update display
  heltec_display_update();
  
  // Print detailed packet info to Serial
  SensorSentinel_print_packet_info(&packet, false);
  // Print the packet as JSON
  SensorSentinel_print_packet_json(&packet); 

  Serial.println("---------------------------\n");
}

/**
 * Create and send a GNSS packet
 */
void sendGnssPacket() {
  // Create and initialize GNSS packet
  SensorSentinel_gnss_packet_t packet;
  bool hasFix = SensorSentinel_init_gnss_packet(&packet, gnssPacketCounter++);
  
  // Show basic info on display
  heltec_clear_display();
  both.println("\nSending Pkt: GNSS");
  both.printf("Packet #%u\n", packet.messageCounter);
  both.printf("Battery: %u%% (%.2fV)\n", 
             packet.batteryLevel, 
             packet.batteryVoltage / 1000.0f);
  
  // Show location info if available
  if (hasFix) {
    both.printf("GPS: %.5f, %.5f\n", packet.latitude, packet.longitude);
    both.printf("HDOP: %.1f\n", packet.hdop / 10.0f);
  } else {
    both.println("GPS: No fix");
  }
  
  heltec_display_update();
  
  // Turn on LED during transmission
  heltec_led(25);
  
  // Send the packet using RadioLib
  int state = radio.transmit((uint8_t*)&packet, sizeof(packet));
  
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("GNSS packet sent successfully!");
    both.println("GNSS packet sent OK");
  } else {
    Serial.printf("ERROR: Transmission failed: %d\n", state);
    both.printf("ERROR: TX failed: %d\n", state);
  }
  
  // Turn off LED
  heltec_led(0);
  
  // Update display
  heltec_display_update();
  
  // Print detailed packet info to Serial
  SensorSentinel_print_packet_info(&packet, false);
  // Print the packet as JSON
  SensorSentinel_print_packet_json(&packet);  

  Serial.println("---------------------------\n");
}