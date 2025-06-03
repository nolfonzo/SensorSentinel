/**
 * @file heltec_receive_sensor_packet.cpp
 * @brief Sensor and GNSS packet receiver for Heltec boards
 * 
 * Listens for incoming packets and displays them using helper functions.
 * Uses the packet library for validation and detailed output.
 */

#include "heltec_unofficial.h"
#include "heltec_sensor_packet.h"

// Configuration
#define MAX_PACKET_SIZE 256  // Maximum packet size we can handle

// Global variables
uint8_t packetBuffer[MAX_PACKET_SIZE];  // Buffer to hold received packet
bool packetReceived = false;
unsigned long lastPacketTime = 0;
uint32_t packetsReceived = 0;

// Function prototypes
void onBinaryPacketReceived(uint8_t* data, size_t length, float rssi, float snr);

void setup() {
  // Initialize the Heltec board
  heltec_setup();
  
  // Clear display and show startup message
  heltec_clear_display();
  both.println("\nPacket Receiver");
  both.printf("Board: %s\n", heltec_get_board_name());
  both.printf("Battery: %d%% (%.2fV)\n", 
             heltec_battery_percent(), 
             heltec_vbat());
  
  // Subscribe to binary packet reception
  if (heltec_subscribe_binary_packets(onBinaryPacketReceived)) {
    both.println("Subscribed to packets");
  } else {
    both.println("Subscribe failed!");
  }
  heltec_display_update();
  delay(2000);
}

void loop() {
  // Handle system tasks (including packet processing)
  heltec_loop();
}

/**
 * Callback for when a binary packet is received
 */
void onBinaryPacketReceived(uint8_t* data, size_t length, float rssi, float snr) {
  // Turn on LED to indicate reception
  heltec_led(25);
  
  // Check packet size
  if (length > MAX_PACKET_SIZE) {
    Serial.printf("ERROR: Packet too large: %u bytes\n", length);
    heltec_clear_display();
    both.println("\nPacket too large!");
    both.printf("Size: %u bytes (max %u)\n", length, MAX_PACKET_SIZE);
    both.printf("RSSI: %.1f dB, SNR: %.1f dB\n", rssi, snr);
    heltec_display_update();
    heltec_led(0);
    return;
  }
  
  // Store packet in buffer
  memcpy(packetBuffer, data, length);
  
  // Basic validation - just check if it's a known message type with the right size
  uint8_t messageType = data[0];  // First byte is message type
  size_t expectedSize = heltec_get_packet_size(messageType);
  
  if (expectedSize == 0 || length != expectedSize) {
    // Unknown packet type or incorrect size
    Serial.printf("ERROR: Unknown packet type (0x%02X) or wrong size (got %u, expected %u)\n", 
                 messageType, length, expectedSize);
    
    heltec_clear_display();
    both.println("\nInvalid packet!");
    both.printf("Type: 0x%02X, Size: %u\n", messageType, length);
    both.printf("RSSI: %.1f dB, SNR: %.1f dB\n", rssi, snr);
    heltec_display_update();
    heltec_led(0);
    return;
  }
  
  // Validate the packet contents
  if (!heltec_validate_packet(packetBuffer, length, true)) {
    Serial.println("ERROR: Packet validation failed");
    heltec_clear_display();
    both.println("\nInvalid packet data!");
    both.printf("RSSI: %.1f dB, SNR: %.1f dB\n", rssi, snr);
    heltec_display_update();
    heltec_led(0);
    return;
  }
  
  // Use the library functions for detailed output to Serial
  heltec_print_packet_info(packetBuffer, true);  
  heltec_print_packet_json(packetBuffer, true);
   // Print divider line
  Serial.println("---------------------------\n"); 

  // Display a simple summary on the OLED
  heltec_clear_display();
  display.println("\nPacket Received!");
  display.printf("RSSI: %.1f dB, SNR: %.1f dB\n", rssi, snr);
  // For the OLED, just show minimal info without type-specific processing
  const char* typeStr = (messageType == HELTEC_MSG_SENSOR) ? "Sensor" : 
                        (messageType == HELTEC_MSG_GNSS) ? "GNSS" : "Unknown";
  
  display.printf("Type: %s\n", typeStr);
  display.printf("Size: %u bytes\n", length);
  display.printf("Total Rx: %u\n", packetsReceived + 1);
  heltec_display_update();
  
  // Update counters and timers
  packetsReceived++;
  lastPacketTime = millis();
  packetReceived = true;
  

  
  // Turn off LED
  heltec_led(0);
}