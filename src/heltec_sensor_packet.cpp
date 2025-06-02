/**
 * @file Heltec_Sensor_Packet.cpp
 * @brief Implementation of packet handling functions for Heltec boards
 */

#include "Heltec_Sensor_Packet.h"
#include "heltec_unofficial.h"
#include <string.h>  // For memcpy

// Get a unique node ID based on ESP32's MAC address
uint32_t heltec_get_node_id() {
  // Use WiFi MAC address if WiFi is available
  #if defined(ESP32) && !defined(HELTEC_NO_WIFI)
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    
    // Create a 32-bit ID from the MAC (using the last 4 bytes)
    return ((uint32_t)mac[2] << 24) | ((uint32_t)mac[3] << 16) | 
           ((uint32_t)mac[4] << 8) | mac[5];
  #else
    // Fallback to a generated ID if WiFi is not available
    uint32_t chipId = 0;
    for(int i=0; i<17; i=i+8) {
      chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
    }
    return chipId;
  #endif
}

// Initialize a sensor packet with basic information
void heltec_init_sensor_packet(heltec_sensor_packet_t* packet, bool includeGnss, uint32_t counter) {
  if (packet == nullptr) return;
  
  // Clear the entire packet first
  memset(packet, 0, sizeof(heltec_sensor_packet_t));
  
  // Determine if GNSS is available based solely on the HELTEC_GNSS flag
  #ifdef HELTEC_GNSS
    bool gnssAvailable = true;
  #else
    bool gnssAvailable = false;
  #endif
  
  // Set message type and size based on GNSS availability
  packet->messageType = (includeGnss && gnssAvailable) ? HELTEC_MSG_GNSS : HELTEC_MSG_BASIC;
  packet->packetSize = heltec_get_packet_size(packet->messageType);
  
  // Set basic packet information
  packet->nodeId = heltec_get_node_id();
  packet->messageCounter = counter;
  
  // Set timestamp to uptime in seconds or use real time if available
  #if defined(ESP32) && defined(HELTEC_USE_RTC_TIME)
    // Use RTC time if available and requested
    struct timeval tv;
    gettimeofday(&tv, NULL);
    packet->timestamp = tv.tv_sec;
  #else
    // Otherwise use uptime
    packet->timestamp = millis() / 1000;
  #endif
  
  // Set system status from heltec_unofficial functions
  packet->batteryPercent = heltec_battery_percent();
  packet->batteryMv = heltec_vbat() * 1000;  // Convert to millivolts
  
  // Set RSSI and SNR to 0, will be filled by receiver
  packet->rssi = 0;
  packet->snr = 0;
  
  // GNSS data if available and requested
  #ifdef HELTEC_GNSS
  if (includeGnss) {
    // Update GNSS data
    heltec_gnss_update();
    
    if (gps.location.isValid()) {
      packet->latitude = gps.location.lat();
      packet->longitude = gps.location.lng();
      packet->altitude = gps.altitude.isValid() ? gps.altitude.meters() : 0;
      packet->satellites = gps.satellites.isValid() ? gps.satellites.value() : 0;
      packet->hdop = gps.hdop.isValid() ? gps.hdop.value() * 10 : 0; // Store as uint8_t * 10
    }
  }
  #endif
}

// Get the size of a packet based on its message type
size_t heltec_get_packet_size(uint8_t messageType) {
  switch (messageType) {
    case HELTEC_MSG_BASIC:
      // Size up to the end of the basic fields (exclude GNSS fields)
      return offsetof(heltec_sensor_packet_t, latitude);
      
    case HELTEC_MSG_GNSS:
      // Full packet size
      return sizeof(heltec_sensor_packet_t);
      
    default:
      // Unknown message type, return 0 to indicate error
      return 0;
  }
}

// Print packet information to Serial for debugging
void heltec_print_packet_info(const heltec_sensor_packet_t* packet, bool verbose) {
  if (packet == nullptr) {
    both.println("Error: Null packet pointer");
    return;
  }
  
  Serial.println("=== Sensor Packet Info ===");
  Serial.printf("Type: 0x%02X (%s)\n", packet->messageType, 
    packet->messageType == HELTEC_MSG_BASIC ? "Basic" : 
    packet->messageType == HELTEC_MSG_GNSS ? "GNSS" : "Unknown");
  Serial.printf("Size: %d bytes\n", packet->packetSize);
  Serial.printf("Node ID: 0x%08X\n", packet->nodeId);
  Serial.printf("Message #: %u\n", packet->messageCounter);
  Serial.printf("Timestamp: %u seconds\n", packet->timestamp);
  
  Serial.printf("Battery: %u%% (%u mV)\n", packet->batteryPercent, packet->batteryMv);
  Serial.printf("RSSI: %d dBm, SNR: %u\n", packet->rssi, packet->snr);
  
  if (verbose) {
    // Print detailed pin information
    Serial.println("Pin Readings:");
    Serial.printf("  Digital: 0x%08X\n", packet->pins.boolean);
    Serial.print("  Analog: [");
    for (int i = 0; i < HELTEC_ANALOG_COUNT; i++) {
      Serial.printf("%u%s", packet->pins.analog[i], 
                i < HELTEC_ANALOG_COUNT-1 ? ", " : "");
    }
    Serial.println("]");
  }
  
  // Print GNSS data if included
  if (packet->messageType == HELTEC_MSG_GNSS) {
    Serial.println("GNSS Data:");
    Serial.printf("  Lat: %.6f, Lon: %.6f\n", packet->latitude, packet->longitude);
    Serial.printf("  Alt: %d m, Satellites: %u\n", packet->altitude, packet->satellites);
    Serial.printf("  HDOP: %.1f\n", packet->hdop / 10.0f);
  }
  
  Serial.println("=========================");
  
  // Ensure display is updated if needed
  heltec_display_update();
}

// Parse a received packet buffer into a packet structure
bool heltec_parse_packet(uint8_t* buffer, size_t length, heltec_sensor_packet_t* packet) {
  if (buffer == nullptr || packet == nullptr) {
    return false;
  }
  
  // Ensure minimum packet size
  if (length < offsetof(heltec_sensor_packet_t, latitude)) {
    Serial.printf("Error: Packet too small (%u bytes)\n", length);
    return false;
  }
  
  // Read message type
  uint8_t messageType = buffer[0];
  
  // Validate message type
  if (messageType != HELTEC_MSG_BASIC && messageType != HELTEC_MSG_GNSS) {
    Serial.printf("Error: Invalid message type 0x%02X\n", messageType);
    return false;
  }
  
  // Get expected packet size based on message type
  size_t expectedSize = heltec_get_packet_size(messageType);
  
  // Validate packet size
  if (length < expectedSize) {
    Serial.printf("Error: Incomplete packet (%u bytes, expected %u)\n", length, expectedSize);
    return false;
  }
  
  // Copy only the relevant portion of the packet based on message type
  memcpy(packet, buffer, expectedSize);
  
  // Validate the packet
  return heltec_validate_packet(packet);
}

// Validate a packet structure for consistency
bool heltec_validate_packet(const heltec_sensor_packet_t* packet) {
  if (packet == nullptr) {
    return false;
  }
  
  // Check message type
  if (packet->messageType != HELTEC_MSG_BASIC && packet->messageType != HELTEC_MSG_GNSS) {
    Serial.printf("Error: Invalid message type 0x%02X\n", packet->messageType);
    return false;
  }
  
  // Check packet size
  size_t expectedSize = heltec_get_packet_size(packet->messageType);
  if (packet->packetSize != expectedSize) {
    Serial.printf("Error: Invalid packet size (%u, expected %u)\n", 
                 packet->packetSize, expectedSize);
    return false;
  }
  
  // Additional validations if needed (e.g., range checks for battery, RSSI, etc.)
  if (packet->batteryPercent > 100) {
    Serial.printf("Warning: Invalid battery percentage (%u%%)\n", packet->batteryPercent);
    // Not fatal, don't return false
  }
  
  // GNSS validation
  if (packet->messageType == HELTEC_MSG_GNSS) {
    // Basic range check for latitude (-90 to 90)
    if (packet->latitude < -90.0f || packet->latitude > 90.0f) {
      Serial.printf("Error: Invalid latitude (%.6f)\n", packet->latitude);
      return false;
    }
    
    // Basic range check for longitude (-180 to 180)
    if (packet->longitude < -180.0f || packet->longitude > 180.0f) {
      Serial.printf("Error: Invalid longitude (%.6f)\n", packet->longitude);
      return false;
    }
  }
  
  // All validations passed
  return true;
}

// Copy packet data from source to destination
void heltec_copy_packet(heltec_sensor_packet_t* dest, const heltec_sensor_packet_t* src) {
  if (dest == nullptr || src == nullptr) return;
  
  // Get the appropriate size based on message type
  size_t copySize = heltec_get_packet_size(src->messageType);
  
  // Copy only the relevant portion of the packet
  memcpy(dest, src, copySize);
}

#ifdef HELTEC_SENSOR_JSON_SUPPORT
// Convert packet to JSON string

String heltec_packet_to_json(const heltec_sensor_packet_t* packet, bool pretty) {
  if (packet == nullptr) {
    return "{}";
  }
  
  // Create a JSON document with appropriate capacity
  // Size depends on whether we're including GNSS data and pretty printing
  const size_t capacity = (packet->messageType == HELTEC_MSG_GNSS) ? 512 : 384;
  
  // Use JsonDocument with capacity
  JsonDocument doc;
  
  // Populate the document
  if (heltec_packet_to_json_doc(packet, doc)) {
    // Add board information from heltec_unofficial
    doc["board"] = heltec_get_board_name();
    
    // Serialize to string
    String result;
    if (pretty) {
      serializeJsonPretty(doc, result);
    } else {
      serializeJson(doc, result);
    }
    return result;
  }
  
  // Return empty JSON if conversion failed
  return "{}";
}

// Populate an existing JsonDocument with packet data
bool heltec_packet_to_json_doc(const heltec_sensor_packet_t* packet, JsonDocument& doc) {
  if (packet == nullptr) {
    return false;
  }
  
  // Add basic packet information
  doc["type"] = packet->messageType;
  doc["type_name"] = (packet->messageType == HELTEC_MSG_BASIC) ? "basic" : 
                    (packet->messageType == HELTEC_MSG_GNSS) ? "gnss" : "unknown";
  doc["node_id"] = packet->nodeId;
  
  // Format node_id as hex string for easier reading
  char nodeIdHex[11];
  sprintf(nodeIdHex, "0x%08X", packet->nodeId);
  doc["node_id_hex"] = nodeIdHex;
  
  doc["counter"] = packet->messageCounter;
  doc["timestamp"] = packet->timestamp;
  
  // Add battery and signal information
  doc["battery"] = packet->batteryPercent;
  doc["battery_mv"] = packet->batteryMv;
  doc["rssi"] = packet->rssi;
  doc["snr"] = packet->snr;
  
  // Add pin readings
  JsonObject pins = doc["pins"].to<JsonObject>();
  
  // Add digital pins with proper board-specific GPIO numbers
  JsonObject digitalObj = pins["digital"].to<JsonObject>();
  
  // Add raw byte value for reference
  char rawHex[5];
  sprintf(rawHex, "0x%02X", packet->pins.boolean);
  digitalObj["raw_byte"] = packet->pins.boolean;
  digitalObj["raw_hex"] = rawHex;
  
  // Get the actual GPIO pin numbers for this board
  for (int i = 0; i < HELTEC_BOOLEAN_COUNT; i++) {
    int pinNumber = heltec_get_boolean_pin(i);
    if (pinNumber >= 0) {
      char pinName[16];
      sprintf(pinName, "gpio_%d", pinNumber);
      bool value = (packet->pins.boolean & (1 << i)) != 0;
      digitalObj[pinName] = value;
    }
  }
  
  // Add analog pins with their actual GPIO numbers
  JsonObject analogObj = pins["analog"].to<JsonObject>();
  
  for (int i = 0; i < HELTEC_ANALOG_COUNT; i++) {
    int pinNumber = heltec_get_analog_pin(i);
    if (pinNumber >= 0) {
      char pinName[16];
      sprintf(pinName, "gpio_%d", pinNumber);
      analogObj[pinName] = packet->pins.analog[i];
    }
  }
  
  // Add GNSS data if available
  if (packet->messageType == HELTEC_MSG_GNSS) {
    JsonObject location = doc["location"].to<JsonObject>();
    location["lat"] = packet->latitude;
    location["lon"] = packet->longitude;
    location["alt"] = packet->altitude;
    location["satellites"] = packet->satellites;
    location["hdop"] = packet->hdop / 10.0f;  // Convert to float (stored as uint8_t * 10)
  }
  
  // Add chip temperature if available
  float temp = heltec_temperature();
  if (temp > -100.0f) {  // Valid temperature
    doc["temperature"] = temp;
  }
  
  return true;
}
#endif // HELTEC_SENSOR_JSON_SUPPORT