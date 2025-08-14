/**
 * @file SensorSentinel_receiver_fwd_mqtt.cpp
 * @brief Sensor and GNSS packet receiver for Heltec boards with MQTT forwarding
 *
 * Listens for incoming packets, displays them, and forwards them to MQTT topics:
 * - Sensor packets go to lora/sensor
 * - GNSS packets go to lora/gnss
 * Uses the SensorSentinel_mqtt_helper for MQTT connectivity.
 */

#include "heltec_unofficial_revised.h"
#include "SensorSentinel_packet_helper.h"
#include "SensorSentinel_mqtt_helper.h"
#include "SensorSentinel_wifi_helper.h"

#ifndef NO_RADIOLIB
#include "SensorSentinel_RadioLib_helper.h"
#endif

// Global variables
uint8_t packetBuffer[MAX_LORA_PACKET_SIZE]; // Buffer to hold received packet
unsigned long lastPacketTime = 0;
uint32_t packetsReceived = 0;
uint32_t packetsForwarded = 0;

// Variables for packet processing
String packetMessageType; // Current packet message type

#define PACKET_CACHE_SIZE 50  // Remember last 50 packets

// Packet cache to prevent duplicate MQTT messages
struct ReceivedPacketCache {
    uint32_t nodeId;
    uint32_t messageCounter;
    unsigned long timestamp;
};
ReceivedPacketCache receivedCache[PACKET_CACHE_SIZE];
uint8_t receivedCacheIndex = 0;

// Function prototypes
void onBinaryPacketReceived(uint8_t *data, size_t length, float rssi, float snr);
bool isPacketAlreadyProcessed(uint32_t nodeId, uint32_t messageCounter);
void addToReceivedCache(uint32_t nodeId, uint32_t messageCounter);

#define STARTUP_DISPLAY_DELAY 2000

void setup()
{
  // Initialize the Heltec board
  heltec_setup();

  Serial.printf("Sensor packet size: %d bytes\n", sizeof(SensorSentinel_sensor_packet_t));
  Serial.printf("Pin readings size: %d bytes\n", sizeof(SensorSentinel_pin_readings_t));
  Serial.printf("GNSS packet size: %d bytes\n", sizeof(SensorSentinel_gnss_packet_t));

  heltec_clear_display();
  both.println("Packet Receiver+MQTT");
  both.printf("Board: %s\n", heltec_get_board_name());
  both.printf("Battery: %d%% (%.2fV)\n",
              heltec_battery_percent(),
              heltec_vbat());

// Subscribe to binary packet reception
#ifndef NO_RADIOLIB
  if (SensorSentinel_subscribe(NULL, onBinaryPacketReceived))
  {
    both.println("Subscribed to packets");
  }
  else
  {
    both.println("Subscribe failed!");
  }
#else
  both.println("No radio, No sub");
#endif


  // Let the helper libraries handle WiFi and MQTT connection
  SensorSentinel_wifi_begin();
  SensorSentinel_mqtt_setup(true); // With time sync

  // Add IP address display before the display update
  if (SensorSentinel_wifi_connected()) {
    both.printf("IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    both.println("No WiFi");
  }

  heltec_display_update();
  delay(STARTUP_DISPLAY_DELAY);
  // Initialize received packet cache
  memset(receivedCache, 0, sizeof(receivedCache));
}

void loop()
{
  // Handle system tasks
  heltec_loop();

#ifndef NO_RADIOLIB
  // Process any pending packet receptions
  SensorSentinel_process_packets();
#endif

  // Let helpers maintain WiFi and MQTT connections
  SensorSentinel_wifi_maintain();
  SensorSentinel_mqtt_maintain();
}

/**
 * Callback for when a binary packet is received
 */

void onBinaryPacketReceived(uint8_t *data, size_t length, float rssi, float snr)
{
  // Turn on LED to indicate reception
  heltec_led(25);

  // Clear display for output
  heltec_clear_display();

   // Store packet in buffer
  memcpy(packetBuffer, data, length);

  // Basic validation - check if it's a known message type with the right size
  bool isValidPacket = SensorSentinel_validate_packet(packetBuffer, length);

  if (isValidPacket)
  {
    uint8_t messageType = *((uint8_t *)data);
    packetMessageType = SensorSentinel_message_type_to_string(messageType);

    // Display the extracted information
    both.printf("\nReceived Type: %s\n", packetMessageType.c_str());
    both.printf("Msg #: %u\n", SensorSentinel_get_message_counter_from_packet(packetBuffer));
    both.printf("NodeID: %u\n", SensorSentinel_extract_node_id_from_packet(packetBuffer));

    // Common display elements (outside the if/else block)
    both.printf("RSSI: %.1f dB\n", rssi);
    both.printf("Size: %u bytes\n", length);
    both.printf("Total Rx: %u\n", packetsReceived + 1);

    // Serial output for valid packets
    SensorSentinel_print_packet_info(packetBuffer, length);
    Serial.println("---------------------------");

    // Check if the packet has already been processed
    uint32_t nodeId = SensorSentinel_extract_node_id_from_packet(packetBuffer);
    uint32_t messageCounter = SensorSentinel_get_message_counter_from_packet(packetBuffer);

    if (!isPacketAlreadyProcessed(nodeId, messageCounter)) {
        Serial.printf("NEW: Processing packet from Node %u (Msg #%u)\n", nodeId, messageCounter);
        
        // Add to cache BEFORE processing to prevent race conditions
        addToReceivedCache(nodeId, messageCounter);
        
        // Forward the packet to MQTT using new helper function
        MqttForwardStatus mqttStatus = SensorSentinel_mqtt_forward_packet(packetBuffer, length, rssi, snr);
        
        // Display MQTT status
        both.printf("MQTT: %s\n", SensorSentinel_mqtt_status_to_string(mqttStatus));
        
        if (mqttStatus == MQTT_SUCCESS) {
            packetsForwarded++;
            // Serial output for packet statistics
            Serial.printf("\nPackets received: %u, Forwarded: %u\n", packetsReceived + 1, packetsForwarded);
            Serial.println("---------------------------");
            Serial.println("---------------------------\n\n");
          }
    } else {
        Serial.printf("DUPLICATE: Already processed packet from Node %u (Msg #%u) - SKIPPING MQTT\n", 
                      nodeId, messageCounter);
        both.println("Duplicate - SKIPPED");
    }

    heltec_display_update();

    // Update counters and timers
    packetsReceived++;
    lastPacketTime = millis();

    // Turn off LED
    heltec_led(0);
  } // ← This closes the if (isValidPacket) block
} // ← ADD THIS MISSING CLOSING BRACE FOR onBinaryPacketReceived!

/**
 * Check if we've already processed this packet
 */
bool isPacketAlreadyProcessed(uint32_t nodeId, uint32_t messageCounter) {
    for (int i = 0; i < PACKET_CACHE_SIZE; i++) {
        if (receivedCache[i].nodeId == nodeId && 
            receivedCache[i].messageCounter == messageCounter) {
            return true; // Already processed
        }
    }
    return false; // New packet
}

/**
 * Remember that we've processed this packet
 */
void addToReceivedCache(uint32_t nodeId, uint32_t messageCounter) {
    receivedCache[receivedCacheIndex].nodeId = nodeId;
    receivedCache[receivedCacheIndex].messageCounter = messageCounter;
    receivedCache[receivedCacheIndex].timestamp = millis();
    
    receivedCacheIndex = (receivedCacheIndex + 1) % PACKET_CACHE_SIZE;
}