/**
 * @file SensorSentinel_sender.cpp
 * @brief Periodic sensor and GNSS packet sender with optional repeater functionality
 * 
 * Sends sensor packets every 30 seconds and GNSS packets every 90 seconds
 * using the heltec_unofficial_revised and SensorSentinel_packeti_helper libraries.
 * 
 * Can also act as a repeater for other nodes' packets when REPEATER_MODE is enabled.
 */

#include "heltec_unofficial_revised.h"
#include "SensorSentinel_packet_helper.h"

#ifndef NO_RADIOLIB
#include "SensorSentinel_RadioLib_helper.h"
#endif

// Configuration
#define SENSOR_INTERVAL 30000  // Sensor send interval in milliseconds 
#define GNSS_INTERVAL   90000  // GNSS send interval in milliseconds 

// Repeater configuration
#define REPEATER_MODE true          // Enable/disable repeater functionality
#define REPEAT_DELAY_MS 1000        // Wait before repeating to avoid collisions
#define MAX_REPEAT_HOPS 3           // Maximum hops (future use)
#define PACKET_CACHE_SIZE 10        // Remember last N packets to avoid duplicates

// Forward declarations
void sendSensorPacket();
void sendGnssPacket();
void onPacketReceived(uint8_t *data, size_t length, float rssi, float snr);
bool shouldRepeatPacket(uint32_t nodeId, uint32_t messageCounter);
void addToCache(uint32_t nodeId, uint32_t messageCounter);
void repeatPacket(uint8_t *data, size_t length);

// Global variables for original functionality
uint32_t sensorPacketCounter = 0;
uint32_t gnssPacketCounter = 0;
unsigned long lastSensorSendTime = 0;
unsigned long lastGnssSendTime = 0;

// Repeater functionality variables
struct PacketCache {
    uint32_t nodeId;
    uint32_t messageCounter;
    unsigned long timestamp;
};
PacketCache packetCache[PACKET_CACHE_SIZE];
uint8_t cacheIndex = 0;
uint32_t packetsRepeated = 0;

void setup() {
  // Initialize the Heltec board
  heltec_setup();

  // Show startup message
  heltec_clear_display();

  both.println("\nPacket Sender");
  both.printf("Board: %s\n", heltec_get_board_name());
  both.printf("Battery: %d%% (%.2fV)\n", 
             heltec_battery_percent(), 
             heltec_vbat());
  
  // Show send schedule
  both.println("Send Intervals:");
  both.printf("Sensor data: %dsec\n", SENSOR_INTERVAL/1000);
  both.printf("GNSS data: %dsec\n", GNSS_INTERVAL/1000);
  
  // Show repeater status
  both.printf("Repeater: %s\n", REPEATER_MODE ? "ON" : "OFF");

  heltec_display_update();
  delay(2000); // Let the message display for a while

#ifndef NO_RADIOLIB
  // Subscribe to incoming packets for repeating (if enabled)
  if (REPEATER_MODE) {
    if (SensorSentinel_subscribe(NULL, onPacketReceived)) {
      Serial.println("Subscribed for packet repeating");
    } else {
      Serial.println("Failed to subscribe for repeating");
    }
  }
#endif
  
  // Initialize timers with offset to avoid sending both packet types at once
  lastSensorSendTime = millis() - SENSOR_INTERVAL; 
  lastGnssSendTime = millis() - GNSS_INTERVAL + 5000;
  
  // Initialize packet cache
  memset(packetCache, 0, sizeof(packetCache));
}

void loop() {
  // Handle system tasks
  heltec_loop();
  
#ifndef NO_RADIOLIB
  // Process incoming packets for repeating
  if (REPEATER_MODE) {
    SensorSentinel_process_packets();
  }
#endif
  
  // Check if it's time to send a sensor packet
  if (millis() - lastSensorSendTime >= SENSOR_INTERVAL) {
    sendSensorPacket();
    lastSensorSendTime = millis();
  }
 
  #ifdef GNSS  
    // Check if it's time to send a GNSS packet
    if (millis() - lastGnssSendTime >= GNSS_INTERVAL) {
      sendGnssPacket();
      lastGnssSendTime = millis();
    }
  #endif
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
  both.printf("Msg: #%u\n", packet.messageCounter);
  both.printf("NodeID: %u\n", packet.nodeId);
  both.printf("Battery: %u%% (%.2fV)\n", 
             packet.batteryLevel, 
             packet.batteryVoltage / 1000.0f);
  
  // Show repeater stats if enabled
  if (REPEATER_MODE) {
    both.printf("Repeated: %u\n", packetsRepeated);
  }
  
  // Turn on LED during transmission
  heltec_led(25);
  
  #ifndef NO_RADIOLIB
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
  #else
      both.println("No Radio");
  #endif
  
  // Turn off LED
  heltec_led(0);
  
  // Update display
  heltec_display_update();
  
  // Print detailed packet info to Serial
  SensorSentinel_print_packet_info(&packet, false);

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
  both.printf("Msg: #%u\n", packet.messageCounter);
  both.printf("NodeID: %u\n", packet.nodeId);
  both.printf("Battery: %u%% (%.2fV)\n", 
             packet.batteryLevel, 
             packet.batteryVoltage / 1000.0f);
  
  // Show repeater stats if enabled
  if (REPEATER_MODE) {
    both.printf("Repeated: %u\n", packetsRepeated);
  }
  
  heltec_display_update();
  
  // Turn on LED during transmission
  heltec_led(25);
  
  #ifndef NO_RADIOLIB
    // Send the packet using RadioLib
    int state = radio.transmit((uint8_t*)&packet, sizeof(packet));
    
    if (state == RADIOLIB_ERR_NONE) {
      Serial.println("GNSS packet sent successfully!");
      both.println("GNSS packet sent OK");
    } else {
      Serial.printf("ERROR: Transmission failed: %d\n", state);
      both.printf("ERROR: TX failed: %d\n", state);
    }
  #else
      both.println("No Radio");
  #endif
  
  // Turn off LED
  heltec_led(0);
  
  // Update display
  heltec_display_update();
  
  // Print detailed packet info to Serial
  SensorSentinel_print_packet_info(&packet, false);

  Serial.println("---------------------------\n");
}

// ===== REPEATER FUNCTIONALITY =====

/**
 * Callback for incoming packets - handles repeating logic
 */
void onPacketReceived(uint8_t *data, size_t length, float rssi, float snr) {
  if (!REPEATER_MODE) return;
  
  // Validate packet
  if (!SensorSentinel_validate_packet(data, length)) {
    Serial.println("Invalid packet received, not repeating");
    return;
  }
  
  uint32_t senderNodeId = SensorSentinel_extract_node_id_from_packet(data);
  uint32_t messageCounter = SensorSentinel_get_message_counter_from_packet(data);
  
  // Don't repeat our own packets
  if (senderNodeId == SensorSentinel_generate_node_id()) {
    Serial.printf("Ignoring my own packet (Node %u)\n", senderNodeId);
    return;
  }
  
  // Check if we should repeat this packet
  if (shouldRepeatPacket(senderNodeId, messageCounter)) {
    Serial.printf("Received packet from Node %u (Msg #%u), RSSI: %.1f, will repeat\n", 
                  senderNodeId, messageCounter, rssi);
    
    // Add small delay to avoid collisions
    delay(REPEAT_DELAY_MS);
    
    // Repeat the packet
    repeatPacket(data, length);
    
    // Remember this packet to avoid future duplicates
    addToCache(senderNodeId, messageCounter);
    
    // Update stats
    packetsRepeated++;
  } else {
    Serial.printf("Received packet from Node %u (Msg #%u), already repeated\n", 
                  senderNodeId, messageCounter);
  }
}

/**
 * Check if we should repeat a packet (avoid duplicates)
 */
bool shouldRepeatPacket(uint32_t nodeId, uint32_t messageCounter) {
  // Check if we've already seen this packet
  for (int i = 0; i < PACKET_CACHE_SIZE; i++) {
    if (packetCache[i].nodeId == nodeId && 
        packetCache[i].messageCounter == messageCounter) {
      return false; // Already repeated this packet
    }
  }
  
  // TODO: Add hop count check here when implemented in packet structure
  
  return true;
}

/**
 * Add packet to cache to remember we've repeated it
 */
void addToCache(uint32_t nodeId, uint32_t messageCounter) {
  packetCache[cacheIndex].nodeId = nodeId;
  packetCache[cacheIndex].messageCounter = messageCounter;
  packetCache[cacheIndex].timestamp = millis();
  
  cacheIndex = (cacheIndex + 1) % PACKET_CACHE_SIZE;
}

/**
 * Retransmit a packet as-is with display feedback
 */
void repeatPacket(uint8_t *data, size_t length) {
  // Clear display and show repeating status
  heltec_clear_display();
  
  // Extract info from packet for display
  uint32_t senderNodeId = SensorSentinel_extract_node_id_from_packet(data);
  uint32_t messageCounter = SensorSentinel_get_message_counter_from_packet(data);
  
  both.println("\nREPEATING PACKET");
  both.printf("From Node: %u\n", senderNodeId);
  both.printf("Msg: #%u\n", messageCounter);
  both.printf("Size: %u bytes\n", length);
  both.printf("Total repeated: %u\n", packetsRepeated + 1);
  
  // Turn on LED during repeat transmission
  heltec_led(50); // Brighter LED for repeating
  
#ifndef NO_RADIOLIB
  // Actually transmit the packet
  int state = radio.transmit(data, length);
  
  // Update display with result
  if (state == RADIOLIB_ERR_NONE) {
    both.println("Repeat SUCCESS");
    Serial.printf("Successfully repeated packet from Node %u (%u bytes)\n", senderNodeId, length);
  } else {
    both.printf("❌ Repeat FAILED: %d\n", state);
    Serial.printf("Failed to repeat packet from Node %u: %d\n", senderNodeId, state);
  }
#else
  both.println("No Radio, no repeat");
  Serial.printf("Would repeat packet from Node %u (%u bytes) but NO_RADIOLIB defined\n", senderNodeId, length);
#endif
  
  // Turn off LED
  heltec_led(0);
  
  // Update display with final status
  heltec_display_update();
}