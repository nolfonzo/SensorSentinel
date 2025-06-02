/**
 * @file heltec_send_sensor_packet.cpp
 * @brief Simple periodic sensor packet sender for Heltec boards
 * 
 * This file demonstrates how to create and send sensor packets
 * using heltec_unofficial and Heltec_Sensor_Packet libraries.
 */

#include "heltec_unofficial.h"
#include "Heltec_Sensor_Packet.h"

// Configuration
#define SEND_INTERVAL  30000  // Send interval in milliseconds (30 seconds)

// Function prototypes
void sendPacket(const heltec_sensor_packet_t* packet);

// Global variables
uint32_t packetCounter = 0;
unsigned long lastSendTime = 0;

void setup() {
  // Initialize the Heltec board
  heltec_setup();
  
  // Clear display and show startup message
  heltec_clear_display(1, 0);
  both.println("\nSensor Packet Sender");
  both.printf("Board: %s\n", heltec_get_board_name());
  both.printf("Battery: %d%%\n", heltec_battery_percent());
  both.printf("Sending Init Packet\n");
  heltec_display_update();
  delay(3000);

  // Create and send initial packet
  heltec_sensor_packet_t packet;
  heltec_init_sensor_packet(&packet, true, packetCounter++);
  sendPacket(&packet);
}

void loop() {
  // Handle system tasks
  heltec_loop();
  
  // Check if button was clicked to trigger immediate send
  if (heltec_button_clicked()) {
    // Show button press on display
    heltec_clear_display(1, 0);
    both.println("\nSensor Packet Sender");
    both.println("Button pressed!");
    both.println("Sending packet...");
  //  heltec_display_update();
    
    // Create and send packet
    heltec_sensor_packet_t packet;
    heltec_init_sensor_packet(&packet, true, packetCounter++);
    sendPacket(&packet);
    
    // Update last send time
    lastSendTime = millis();
  }
  
  // Check if it's time to send a periodic packet
  if (millis() - lastSendTime >= SEND_INTERVAL) {
    // Create and send packet
    heltec_sensor_packet_t packet;
    heltec_init_sensor_packet(&packet, true, packetCounter++);
    sendPacket(&packet);
    
    // Update last send time
    lastSendTime = millis();
  }
  
  // Use heltec_delay to ensure power button functionality works
  heltec_delay(10);
}

/**
 * Send a packet using the radio
 */
void sendPacket(const heltec_sensor_packet_t* packet) {
  if (packet == nullptr) return;
  
  // Get packet size based on type
  size_t packetSize = heltec_get_packet_size(packet->messageType);
  
  // Clear display and show transmission info
  heltec_clear_display(1, 0);

  Serial.println("\n--- Sending Packet ---");
  both.printf("Sending packet #%d\n", packet->messageCounter);
  both.printf("Size: %d bytes\n", packetSize);
  both.printf("Battery: %d%%\n", packet->batteryPercent);
  heltec_display_update();
  
  // Print packet as JSON to Serial for debugging
  #ifdef HELTEC_SENSOR_JSON_SUPPORT
    Serial.println("JSON:");
    Serial.println(heltec_packet_to_json(packet, true));
  #else
    // Fallback if JSON support is not available
    Serial.println("No JSON Support, packet info:");
    heltec_print_packet_info(packet, true);
  #endif
  
  // Turn on LED during transmission
  heltec_led(25);
  
  #ifndef HELTEC_NO_RADIOLIB
  // Send the packet using RadioLib
  int state = radio.transmit((uint8_t*)packet, packetSize);
  
  if (state == RADIOLIB_ERR_NONE) {
    // Display message type before success message
    both.printf("Message Type: %s\n", 
      packet->messageType == HELTEC_MSG_GNSS ? "GNSS" : "NO GNSS");
    both.println("Packet sent OK");
    Serial.println("---------------------------\n");

    // Show GNSS info if this is a GNSS packet
    if (packet->messageType == HELTEC_MSG_GNSS) {
      if (packet->satellites > 0) {
        both.printf("GPS: %.5f, %.5f\n", packet->latitude, packet->longitude);
        both.printf("Sats: %d\n", packet->satellites);
      } else {
        both.println("GPS: No fix");
      }
    }
  } else {
    both.printf("Transmission failed: %d\n", state);
  }
  #else
  // RadioLib disabled - just print packet info
  both.printf("Message Type: %s\n", 
    packet->messageType == HELTEC_MSG_GNSS ? "GNSS" : "NO GNSS");
  heltec_print_packet_info(packet, true);
  #endif
  
  // Turn off LED
  heltec_led(0);
  
  // Ensure display updates
  heltec_display_update();
}