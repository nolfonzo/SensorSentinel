/**
 * @file heltec_receive_sensor_packet_fwd_mqtt.cpp
 * @brief Sensor and GNSS packet receiver for Heltec boards with MQTT forwarding
 * 
 * Listens for incoming packets, displays them, and forwards them to MQTT topics:
 * - Sensor packets go to lora/sensor
 * - GNSS packets go to lora/gnss
 * Uses the heltec_mqtt_gateway for MQTT connectivity.
 */

#include "heltec_unofficial.h"
#include "heltec_sensor_packet_helper.h"
#include "heltec_mqtt_gateway.h"
#include "heltec_wifi_helper.h"
#include <ArduinoJson.h>

// Configuration
#define MAX_LORA_PACKET_SIZE 256  // Maximum packet size we can handle

// Status update frequency (in seconds)
#ifndef MQTT_STATUS_FREQ_SECS
#define MQTT_STATUS_FREQ_SECS 60  // Default to 60 seconds if not defined
#endif

// Global variables
uint8_t packetBuffer[MAX_LORA_PACKET_SIZE];  // Buffer to hold received packet
unsigned long lastPacketTime = 0;
uint32_t packetsReceived = 0;
uint32_t packetsForwarded = 0;

// Variables for packet processing
JsonDocument packetJson;      // Reusable JSON document for packet parsing
String packetMessageType;     // Current packet message type
bool packetJsonValid = false; // Flag indicating if JSON is valid

// Function prototypes
void onBinaryPacketReceived(uint8_t* data, size_t length, float rssi, float snr);
bool forwardPacketToMQTT(uint8_t* data, size_t length, float rssi, float snr);

void setup() {
  // Initialize the Heltec board
  heltec_setup();
  
  // Clear display and show startup message
  heltec_clear_display();
  both.println("Packet Receiver+MQTT");
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
  delay(2000);
  heltec_display_update();
   
  // Let the helper libraries handle WiFi and MQTT connection
  heltec_wifi_begin();
  heltec_mqtt_setup(true);  // With time sync
  
  // Display initial connection status via the MQTT helper
  heltec_mqtt_display_status(0);
}

void loop() {
  // Handle system tasks (including packet processing)
  heltec_loop();
  
  // Let helpers maintain WiFi and MQTT connections
  heltec_wifi_maintain();
  heltec_mqtt_maintain();
  
  // Publish status update to MQTT based on configured frequency
  static unsigned long lastStatusTime = 0;
  if (millis() - lastStatusTime > (MQTT_STATUS_FREQ_SECS * 1000)) {
    if (heltec_mqtt_get_client().connected()) {
      heltec_mqtt_publish_status();
    }
    lastStatusTime = millis();
  }
}

/**
 * Callback for when a binary packet is received
 */
void onBinaryPacketReceived(uint8_t* data, size_t length, float rssi, float snr) {
  // Turn on LED to indicate reception
  heltec_led(25);

   // Clear display for output
  heltec_clear_display();
  both.println("Packet Received!"); 

  // Check packet size
  if (length > MAX_LORA_PACKET_SIZE) {
    Serial.printf("ERROR: Packet too large: %u bytes\n", length);
    heltec_clear_display();
    both.println("Packet too large!");
    both.printf("Size: %u bytes (max %u)\n", length, MAX_LORA_PACKET_SIZE);
    both.printf("RSSI: %.1f dB, SNR: %.1f dB\n", rssi, snr);
    heltec_display_update();
    delay(2000);
    heltec_led(0);
    return;
  }
  
  // Store packet in buffer
  memcpy(packetBuffer, data, length);
  
  // Basic validation - check if it's a known message type with the right size
  bool isValidPacket = heltec_validate_packet(packetBuffer, length, true);
  
  // Calculate time since last packet (for Serial output only)
  if (lastPacketTime > 0) {
    unsigned long timeBetweenPackets = (millis() - lastPacketTime) / 1000;
    Serial.printf("Time since last packet: ");
    if (timeBetweenPackets < 60) {
      Serial.printf("%lu seconds\n", timeBetweenPackets);
    } else if (timeBetweenPackets < 3600) {
      Serial.printf("%lu minutes\n", timeBetweenPackets / 60);
    } else {
      Serial.printf("%lu hours\n", timeBetweenPackets / 3600);
    }
  }
  
  // Reset packet processing flag
  packetJsonValid = false;
  
  if (isValidPacket) {
    // For valid packets, parse the JSON once
    packetJsonValid = heltec_packet_to_json_doc(packetBuffer, packetJson);
    
    if (packetJsonValid) {
      // Extract the data we need
      packetMessageType = packetJson["type"].as<String>();
      uint32_t messageCounter = packetJson["counter"];
      
      // Display the extracted information
      both.printf("Type: %s\n", packetMessageType.c_str());
      both.printf("Msg #: %u\n", messageCounter);
    } else {
      // JSON conversion failed for some reason - this is unexpected!
      both.println("ERROR: JSON conv fail!");
      
      // Print the raw data for debugging
      Serial.printf("Packet that failed JSON conversion: ");
      for (size_t i = 0; i < length; i++) {
        Serial.printf("%02X", packetBuffer[i]);
      }
      Serial.println();
    }
    
    // Serial output for valid packets
    heltec_print_packet_info(packetBuffer, true);  
    heltec_print_packet_json(packetBuffer, true);

  } else {
    // Display output for invalid packets
    both.println("Packet Structure Unknown");
    both.println("Will Fwd Raw Data"); 
    
    // Serial output for invalid packets
    Serial.printf("Raw data (%u bytes): ", length);
    for (size_t i = 0; i < length; i++) {
      Serial.printf("%02X", packetBuffer[i]);
    }
    Serial.println();
  }
  
  // Common display elements (outside the if/else block)
  both.printf("RSSI: %.1f dB,\nSNR: %.1f dB\n", rssi, snr);
  both.printf("Size: %u bytes\n", length);
  both.printf("Total Rx: %u\n", packetsReceived + 1);
  Serial.println("---------------------------");  

  // Forward the packet to MQTT - global variables are used internally
  bool mqttForwarded = forwardPacketToMQTT(packetBuffer, length, rssi, snr);
  
  // Serial output for packet statistics
  Serial.printf("Packets received: %u, Forwarded: %u\n", packetsReceived + 1, packetsForwarded);
  Serial.println("---------------------------"); 
  Serial.println("---------------------------\n"); 
  
  // MQTT status display
  if (!mqttForwarded) {
    both.println("MQTT: Forward failed");
  }
  
  heltec_display_update();
  
  // Update counters and timers
  packetsReceived++;
  lastPacketTime = millis();
  
  // Turn off LED
  heltec_led(0);
}

/**
 * Forward a received packet to the appropriate MQTT topic
 * @param data Pointer to the packet data
 * @param length Length of the packet in bytes
 * @param rssi RSSI value for the received packet
 * @param snr SNR value for the received packet
 * @return true if forwarding was successful, false otherwise
 */
bool forwardPacketToMQTT(uint8_t* data, size_t length, float rssi, float snr) {
  // Skip if MQTT is not connected
  if (!heltec_mqtt_get_client().connected()) {
    Serial.println("MQTT not connected - cannot forward packet");
    return false;
  }
  
  // Validate the packet if we haven't already validated it
  bool isValidPacket = packetJsonValid || heltec_validate_packet(data, length, true);
  
  if (isValidPacket) {
    // For valid packets, use JSON and publish to the appropriate topic
    const char* topic;
    
    // If we don't have valid JSON yet, try to parse it now
    if (!packetJsonValid) {
      JsonDocument localDoc;
      if (heltec_packet_to_json_doc(data, localDoc)) {
        // Successfully parsed JSON in this function
        String msgType = localDoc["type"].as<String>();
        
        // Select topic based on message type
        if (msgType == "sensor") {
          topic = MQTT_TOPIC_SENSOR;
        } else if (msgType == "gnss") {
          topic = MQTT_TOPIC_GNSS;
        } else {
          topic = MQTT_TOPIC_DATA;
        }
        
        // Use the existing publish method
        bool success = heltec_mqtt_publish_json(topic, localDoc, false, true);
        
        if (success) {
          Serial.printf("Successfully forwarded packet to MQTT topic: %s\n", topic);
          packetsForwarded++;
          return true;
        } else {
          Serial.printf("ERROR: Failed to forward packet to MQTT topic: %s\n", topic);
          return false;
        }
      } else {
        // Failed to parse JSON - this is unexpected for a valid packet
        Serial.println("ERROR: Failed to convert valid packet to JSON for MQTT");
        return false;
      }
    } else {
      // We already have valid JSON from the callback function
      
      // Select topic based on message type
      if (packetMessageType == "sensor") {
        topic = MQTT_TOPIC_SENSOR;
      } else if (packetMessageType == "gnss") {
        topic = MQTT_TOPIC_GNSS;
      } else {
        topic = MQTT_TOPIC_DATA;
      }
      
      // Use the existing publish method
      bool success = heltec_mqtt_publish_json(topic, packetJson, false, true);
      
      if (success) {
        Serial.printf("Successfully forwarded packet to MQTT topic: %s\n", topic);
        packetsForwarded++;
        return true;
      } else {
        Serial.printf("ERROR: Failed to forward packet to MQTT topic: %s\n", topic);
        return false;
      }
    }
  } else {
    // For invalid packets, publish raw binary data directly
    bool success = heltec_mqtt_get_client().publish(MQTT_TOPIC_DATA, data, length);
    
    if (success) {
      packetsForwarded++;
      Serial.printf("Forwarded raw data (%u bytes) to %s\n", length, MQTT_TOPIC_DATA);
      return true;
    } else {
      Serial.printf("ERROR: Failed to forward raw data to MQTT topic: %s\n", MQTT_TOPIC_DATA);
      return false;
    }
  }
}