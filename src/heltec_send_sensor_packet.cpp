/**
 * @file heltec_send_sensor_packet.cpp
 * @brief Periodic sensor and GNSS packet sender for Heltec boards
 * 
 * Sends sensor packets every 30 seconds and GNSS packets every 90 seconds
 * using the heltec_unofficial and heltec_sensor_packet libraries.
 */

#include "heltec_unofficial.h"
#include "heltec_sensor_packet.h"

// Configuration
#define SENSOR_INTERVAL 30000  // Sensor send interval in milliseconds (30 seconds)
#define GNSS_INTERVAL   90000  // GNSS send interval in milliseconds (90 seconds)

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
  
  // Initialize timers with offset to avoid sending both packet types at once
  lastSensorSendTime = millis() - (SENSOR_INTERVAL - 5000);  // Send first sensor packet in 5 seconds
  lastGnssSendTime = millis() - (GNSS_INTERVAL - 15000);     // Send first GNSS packet in 15 seconds
  
  delay(2000);
  
  // Show send schedule
  heltec_clear_display();
  both.println("\nSend Schedule");
  both.println("\nIntervals:");
  both.printf("Sensor data: %dsec\n", SENSOR_INTERVAL/1000);
  both.printf("GNSS data: %dsec\n", GNSS_INTERVAL/1000);
  both.println("\nTransmitting...");
  heltec_display_update();
  
  delay(2000);
}

void loop() {
  // Handle system tasks
  heltec_loop();
  
  // Check if it's time to send a sensor packet
  if (millis() - lastSensorSendTime >= SENSOR_INTERVAL) {
    sendSensorPacket();
    lastSensorSendTime = millis();
  }
  
  // Check if it's time to send a GNSS packet
  if (millis() - lastGnssSendTime >= GNSS_INTERVAL) {
    sendGnssPacket();
    lastGnssSendTime = millis();
  }
  
  // Use heltec_delay to ensure power button functionality works
  heltec_delay(10);
}

/**
 * Create and send a sensor packet
 */
void sendSensorPacket() {
  // Create and initialize the sensor packet
  heltec_sensor_packet_t packet;
  bool initSuccess = heltec_init_sensor_packet(&packet, sensorPacketCounter++);

  if (!initSuccess) {
    Serial.println("ERROR: Failed to initialize sensor packet");
    both.println("ERROR: Packet init failed");
    return;
  } 

  // Show basic info on display
  heltec_clear_display();
  both.println("\nSending Sensor Packet");
  both.printf("Packet #%u\n", packet.messageCounter);
  both.printf("Battery: %u%% (%.2fV)\n", 
             packet.batteryLevel, 
             packet.batteryVoltage / 1000.0f);
  heltec_display_update();
  
  // Turn on LED during transmission
  heltec_led(25);
  
  // Send the packet using RadioLib
  int state = radio.transmit((uint8_t*)&packet, sizeof(packet));
  
  if (state == RADIOLIB_ERR_NONE) {
    both.println("Sensor packet sent OK");
  } else {
    both.printf("ERROR: TX failed: %d\n", state);
  }
  
  // Turn off LED
  heltec_led(0);
  
  // Update display
  heltec_display_update();
  
  // Print detailed packet info to Serial
  heltec_print_packet_info(&packet, false);
  // Print the packet as JSON
  heltec_print_packet_json(&packet); 

  Serial.println("---------------------------\n");
}

/**
 * Create and send a GNSS packet
 */
void sendGnssPacket() {
  // Create and initialize GNSS packet
  heltec_gnss_packet_t packet;
  bool hasFix = heltec_init_gnss_packet(&packet, gnssPacketCounter++);
  
  // Show basic info on display
  heltec_clear_display();
  both.println("\nSending GNSS Packet");
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
  heltec_print_packet_info(&packet, false);
  // Print the packet as JSON
  heltec_print_packet_json(&packet);  

  Serial.println("---------------------------\n");
}