/**
 * @file heltec_receive_sensor_packet.cpp
 * @brief Sensor packet receiver with MQTT publishing for Heltec boards
 */

#include "heltec_unofficial.h"
#include "heltec_sensor_packet.h"
#include "heltec_mqtt_gateway.h"      // Include MQTT gateway
#include "heltec_wifi_helper.h"       // For WiFi connectivity
#include "heltec_sensor_mqtt_helper.h" // Include our MQTT helper

// Global variables
heltec_sensor_packet_t lastReceivedPacket;
bool packetReceived = false;
uint32_t packetCounter = 0;
unsigned long lastStatusTime = 0;

// Function prototypes
void onBinaryPacketReceived(uint8_t* data, size_t length, float rssi, float snr);

void setup() {
  // Initialize the Heltec board
  heltec_setup();
  
  // Clear display and show startup message
  heltec_clear_display(1, 0);
  both.println("Sensor Packet Gateway");
  both.printf("Board: %s\n", heltec_get_board_name());
  
  // Initialize WiFi
  both.println("Connecting WiFi...");
  heltec_display_update();
  
  // Connect to WiFi
  if (heltec_wifi_begin()) {
    both.println("WiFi connected");
    both.println(heltec_wifi_ip());
  } else {
    both.println("WiFi failed!");
  }
  
  // Initialize MQTT
  both.println("Setting up MQTT...");
  heltec_display_update();
  
  if (heltec_mqtt_setup(true)) { // true = sync time
    both.println("MQTT ready");
  } else {
    both.println("MQTT setup pending");
  }
  
  // Subscribe to binary packet reception
  if (heltec_subscribe_binary_packets(onBinaryPacketReceived)) {
    both.println("LoRa RX active");
  } else {
    both.println("LoRa subscribe fail");
  }
  
  // Show startup complete
  both.println("Gateway ready!");
  heltec_display_update();
}

void loop() {
  // Handle system tasks (including packet processing)
  heltec_loop();
  
  // Maintain WiFi connection
  heltec_wifi_maintain();
  
  // Maintain MQTT connection
  heltec_mqtt_maintain();
  
  // Publish status every 30 seconds
  if (millis() - lastStatusTime > 300000) {
    lastStatusTime = millis();
    
    // Create extra info document
    JsonDocument extraInfo;
    
    // Add sensor packet information
    heltec_sensor_mqtt_add_status(extraInfo);
    
    // Publish status
    heltec_mqtt_publish_status(packetCounter, &extraInfo);
    
    // Update display with status
    heltec_mqtt_display_status(packetCounter);
  }
  
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
    
    // Increment the packet counter
    packetCounter++;
    
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
    
    // Publish to MQTT
    if (heltec_mqtt_maintain()) { // Check if MQTT is connected
      if (heltec_sensor_mqtt_publish(&lastReceivedPacket, rssi, snr * 10)) {
        both.println("MQTT: Published");
      } else {
        both.println("MQTT: Publish failed");
      }
    } else {
      both.println("MQTT: Not connected");
    }
    
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