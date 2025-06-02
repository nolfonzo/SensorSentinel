/**
 * @file heltec_receive_sensor_packet.cpp
 * @brief Simple sensor packet receiver for Heltec boards
 */

#include "heltec_unofficial.h"
#include "Heltec_Sensor_Packet.h"

// Global variables
heltec_sensor_packet_t lastReceivedPacket;
bool packetReceived = false;

// Function prototypes
void onBinaryPacketReceived(uint8_t* data, size_t length, float rssi, float snr);

void setup() {
  // Initialize the Heltec board
  heltec_setup();
  
  // Clear display and show startup message
  heltec_clear_display(1, 0);
  both.println("Sensor Packet RX");
  both.printf("Board: %s\n", heltec_get_board_name());
  heltec_display_update();
  
  // Subscribe to binary packet reception
  if (heltec_subscribe_binary_packets(onBinaryPacketReceived)) {
    both.println("Subscribe active");
  } else {
    both.println("Subscribe fail");
  }
  
  // Show startup complete
  both.println("Listening for packets...");
  heltec_display_update();
}

void loop() {
  // Handle system tasks (including packet processing)
  heltec_loop();
  
  // Use heltec_delay to ensure power button functionality works
  heltec_delay(10);
}

/**
 * Callback for when a binary packet is received
 */
void onBinaryPacketReceived(uint8_t* data, size_t length, float rssi, float snr) {
  // Turn on LED to indicate reception
  heltec_led(25);
  
  // Parse the packet (directly from binary data)
  if (heltec_parse_packet(data, length, &lastReceivedPacket)) {
    // Update RSSI and SNR in the packet
    lastReceivedPacket.rssi = rssi;
    lastReceivedPacket.snr = snr * 10;  // Store as int (dB * 10)
    
    // Clear display and show basic packet information
    heltec_clear_display(1, 0);
    Serial.println("\n===== RECEIVED PACKET =====");
    both.println("Packet Received!");
    both.printf("#%d\nID: 0x%08X\n", 
              lastReceivedPacket.messageCounter,
              lastReceivedPacket.nodeId);
    both.printf("RSSI: %.1f dB \nSNR: %.1f dB\n", rssi, snr);
    
    // Show message type (just the basic type)
    const char* typeStr = "Unknown";
    if (lastReceivedPacket.messageType == HELTEC_MSG_BASIC) typeStr = "Basic";
    if (lastReceivedPacket.messageType == HELTEC_MSG_GNSS) typeStr = "GNSS";
    both.printf("Type: %s\n", typeStr);
    
    // Print full packet details to Serial
    heltec_print_packet_info(&lastReceivedPacket, true);
    
    #ifdef HELTEC_SENSOR_JSON_SUPPORT
    // Output JSON to serial
    String json = heltec_packet_to_json(&lastReceivedPacket, true);
    Serial.println("\nJSON:");
    Serial.println(json);
    #endif
    
    // Set flag to indicate we have a valid packet
    packetReceived = true;
  } else {
    // Failed to parse - show error
    heltec_clear_display(1, 0);
    both.println("Sensor Packet RX");
    both.println("Invalid packet");
    both.printf("Size: %d bytes\n", length);
    both.printf("RSSI: %.1f dB, \nSNR: %.1f dB\n", rssi, snr);
  }
  
  // Turn off LED
  heltec_led(0);
  
  // Update display
  heltec_display_update();
}