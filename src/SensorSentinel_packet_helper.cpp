/**
 * @file SensorSentinel_Sensor_Packet.cpp
 * @brief Implementation of packet handling functions for Heltec boards
 */

#include "SensorSentinel_packet_helper.h"
#include "heltec_unofficial_revised.h"
#include <ArduinoJson.h>
#include <string.h>  // For memcpy

// Get a unique node ID based on ESP32's MAC address
uint32_t SensorSentinel_get_node_id() {
  // Use WiFi MAC address if WiFi is available
  #if defined(ESP32) && !defined(SensorSentinel_NO_WIFI)
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

/**
 * @brief Initialize a basic sensor packet with device information
 * 
 * Populates a packet with node ID, uptime, pin readings and battery level.
 * 
 * @param packet Pointer to sensor packet structure to initialize
 * @param counter Message sequence counter value
 */
bool SensorSentinel_init_sensor_packet(SensorSentinel_sensor_packet_t* packet, uint32_t counter) {
  if (!packet) return false;
  
  // Clear the structure first
  memset(packet, 0, sizeof(SensorSentinel_sensor_packet_t));
  
  // Set message type
  packet->messageType = SensorSentinel_MSG_SENSOR;  // Using new constant name
  
  // Set basic information
  packet->nodeId = SensorSentinel_get_node_id();
  packet->messageCounter = counter;
  packet->uptime = millis() / 1000;  // Seconds since boot
 
  // Get battery information
  //float batteryVolts = heltec_vbat();
  float batteryVolts = 0.0f;    //TODO, the battery code kills the display of the Tacker
  packet->batteryVoltage = (uint16_t)(batteryVolts * 1000.0f);
  packet->batteryLevel = heltec_battery_percent(batteryVolts);
  
  // Get pin readings
  SensorSentinel_read_all_pins(&packet->pins);
  
  return true;
}

/**
 * @brief Initialize a GNSS packet with device information and location data if available
 * 
 * Populates a packet with node ID, uptime, battery level, and current GNSS data.
 * If no valid GNSS fix is available, location fields are set to zeros.
 * 
 * @param packet Pointer to GNSS packet structure to initialize
 * @param counter Message sequence counter value
 * @return true if valid location data was available, false if no fix
 */
bool SensorSentinel_init_gnss_packet(SensorSentinel_gnss_packet_t* packet, uint32_t counter) {
  if (!packet) return false;
  
  // Clear the structure first
  memset(packet, 0, sizeof(SensorSentinel_gnss_packet_t));
  
  // Set message type
  packet->messageType = SensorSentinel_MSG_GNSS;
  
  // Set basic information
  packet->nodeId = SensorSentinel_get_node_id();
  packet->messageCounter = counter;
  packet->uptime = millis() / 1000;  // Seconds since boot
  
  // Get battery information
  float batteryVolts = heltec_vbat();
  packet->batteryVoltage = (uint16_t)(batteryVolts * 1000.0f);
  packet->batteryLevel = heltec_battery_percent(batteryVolts);
  
  // Initialize location fields to zero (indicating no fix)
  // These will stay at zero if no GNSS data is available
  packet->latitude = 0.0f;
  packet->longitude = 0.0f;
  packet->speed = 0.0f;
  packet->course = 0.0f;
  packet->hdop = 0;
  
  // Attempt to populate with GNSS data if available
  bool hasValidFix = false;
  
  #ifdef SensorSentinel_GNSS
  // Update GNSS data
  SensorSentinel_gnss_update();
  
  if (gps.location.isValid()) {
    packet->latitude = gps.location.lat();
    packet->longitude = gps.location.lng();
    packet->speed = gps.speed.kmph();
    packet->course = gps.course.deg();
    packet->hdop = (uint8_t)(gps.hdop.hdop() * 10);  // Store HDOP as fixed point
    hasValidFix = true;
  }
  #endif
  
  return hasValidFix;
}

/**
 * @brief Get the size of a packet based on its message type
 * 
 * Returns the appropriate size in bytes for a given message type.
 * Useful for allocating the correct buffer size or validating 
 * received packet lengths.
 * 
 * @param messageType The type of message (SensorSentinel_MSG_SENSOR or SensorSentinel_MSG_GNSS)
 * @return The size of the packet in bytes, or 0 if the message type is unknown
 */
size_t SensorSentinel_get_packet_size(uint8_t messageType) {
  switch (messageType) {
    case SensorSentinel_MSG_SENSOR:
      return sizeof(SensorSentinel_sensor_packet_t);
    
    case SensorSentinel_MSG_GNSS:
      return sizeof(SensorSentinel_gnss_packet_t);
    
    default:
      // Unknown message type
      return 0;
  }
}



/**
 * @brief Print detailed information about a packet to Serial
 * 
 * Prints header information common to all packets plus specific
 * information based on the packet type. Useful for debugging
 * and monitoring.
 * 
 * @param packet Pointer to the packet buffer
 * @param showAll Whether to show all fields including reserved bytes
 * @return true if the packet was recognized and printed, false otherwise
 */
bool SensorSentinel_print_packet_info(const void* packet, bool showAll) {
  if (!packet) {
    Serial.println("Error: Null packet pointer");
    return false;
  }

  // Get the message type from the first byte
  uint8_t messageType = *((uint8_t*)packet);
  
  // Print common header information
  Serial.println("------ Packet Information ------");
  
  // Handle different packet types
  switch (messageType) {
    case SensorSentinel_MSG_SENSOR: {
      const SensorSentinel_sensor_packet_t* sensorPacket = (const SensorSentinel_sensor_packet_t*)packet;
      
      // Print header information
      Serial.println("Type: Sensor Data");
      Serial.printf("Node ID: 0x%08X\n", sensorPacket->nodeId);
      Serial.printf("Msg #: %u\n", sensorPacket->messageCounter);
      Serial.printf("Uptime: %u seconds\n", sensorPacket->uptime);
      Serial.printf("Battery: %u%% (%.2fV)\n", 
                    sensorPacket->batteryLevel, 
                    sensorPacket->batteryVoltage / 1000.0f);
      
      // Print analog pin readings
      Serial.println("\nAnalog Readings:");
      for (int i = 0; i < 4; i++) {
        Serial.printf("  A%d: %u\n", i, sensorPacket->pins.analog[i]);
      }
      
      // Print digital pin readings
      Serial.println("\nDigital Readings:");
      for (int i = 0; i < 8; i++) {
        bool pinState = (sensorPacket->pins.boolean >> i) & 0x01;
        Serial.printf("  D%d: %d\n", i, pinState);
      }
      
      // Print reserved bytes if requested
      if (showAll) {
        Serial.println("\nReserved Bytes:");
        for (int i = 0; i < sizeof(sensorPacket->reserved); i++) {
          Serial.printf("  [%d]: 0x%02X\n", i, sensorPacket->reserved[i]);
        }
      }
      
      return true;
    }
    
    case SensorSentinel_MSG_GNSS: {
      const SensorSentinel_gnss_packet_t* gnssPacket = (const SensorSentinel_gnss_packet_t*)packet;
      
      // Print header information
      Serial.println("Type: GNSS Location Data");
      Serial.printf("Node ID: 0x%08X\n", gnssPacket->nodeId);
      Serial.printf("Msg #: %u\n", gnssPacket->messageCounter);
      Serial.printf("Uptime: %u seconds\n", gnssPacket->uptime);
      Serial.printf("Battery: %u%% (%.2fV)\n", 
                    gnssPacket->batteryLevel, 
                    gnssPacket->batteryVoltage / 1000.0f);
      
      // Print location information
      Serial.println("\nLocation Data:");
      Serial.printf("  Latitude: %.6f°\n", gnssPacket->latitude);
      Serial.printf("  Longitude: %.6f°\n", gnssPacket->longitude);
      Serial.printf("  Speed: %.1f km/h\n", gnssPacket->speed);
      Serial.printf("  Course: %.1f°\n", gnssPacket->course);
      Serial.printf("  HDOP: %.1f\n", gnssPacket->hdop / 10.0f);
      
      // Create a Google Maps link
      Serial.println("\nGoogle Maps Link:");
      Serial.printf("  https://maps.google.com/maps?q=%.6f,%.6f\n", 
                    gnssPacket->latitude, gnssPacket->longitude);
      
      // Print reserved bytes if requested
      if (showAll) {
        Serial.println("\nReserved Bytes:");
        for (int i = 0; i < sizeof(gnssPacket->reserved); i++) {
          Serial.printf("  [%d]: 0x%02X\n", i, gnssPacket->reserved[i]);
        }
      }
      
      return true;
    }
    
    default:
      Serial.printf("Unknown packet type: 0x%02X\n", messageType);
      return false;
  }
}



/**
 * @brief Parse binary data into the appropriate packet structure
 * 
 * Examines the message type in the binary data and copies it to the
 * appropriate output structure if the type and size match expectations.
 * Prints detailed error messages to Serial when parsing fails.
 * 
 * @param data Pointer to the binary data buffer
 * @param dataSize Size of the binary data in bytes
 * @param outputPacket Pointer where the parsed packet will be stored
 * @param outputSize Size of the output structure in bytes
 * @return Message type if successful, 0 if parsing failed
 */
uint8_t SensorSentinel_parse_packet(const uint8_t* data, size_t dataSize, void* outputPacket, size_t outputSize) {
  // Check output parameter
  if (!outputPacket) {
    Serial.println("ERROR: Null output packet pointer");
    return 0;
  }
  
  // Basic validation before attempting to read message type
  if (!data || dataSize == 0) {
    Serial.println("ERROR: Invalid input data");
    return 0;
  }
  
  // Get message type and expected size
  uint8_t messageType = data[0];
  size_t expectedSize = SensorSentinel_get_packet_size(messageType);
  
  // Check if the message type is valid before proceeding
  if (expectedSize == 0) {
    Serial.printf("ERROR: Unknown message type 0x%02X\n", messageType);
    return 0;
  }
  
  // Check output buffer size - this is unique to parse and should stay here
  if (outputSize < expectedSize) {
    Serial.printf("ERROR: Output buffer too small - needs %u bytes, got %u bytes\n", 
                 expectedSize, outputSize);
    return 0;
  }
  
  // Now use validate for comprehensive packet validation
  if (!SensorSentinel_validate_packet(data, dataSize, true)) {
    return 0; // Validation failed
  }
  
  // If we got here, validation passed, so copy the data
  memcpy(outputPacket, data, expectedSize);
  
  // Log success based on message type
  if (messageType == SensorSentinel_MSG_SENSOR) {
    SensorSentinel_sensor_packet_t* sensorPacket = (SensorSentinel_sensor_packet_t*)outputPacket;
    Serial.printf("INFO: Successfully parsed SENSOR packet from node 0x%08X (msg #%u)\n", 
                 sensorPacket->nodeId, sensorPacket->messageCounter);
  }
  else if (messageType == SensorSentinel_MSG_GNSS) {
    SensorSentinel_gnss_packet_t* gnssPacket = (SensorSentinel_gnss_packet_t*)outputPacket;
    Serial.printf("INFO: Successfully parsed GNSS packet from node 0x%08X (msg #%u)\n", 
                 gnssPacket->nodeId, gnssPacket->messageCounter);
    Serial.printf("      Location: %.6f, %.6f\n", gnssPacket->latitude, gnssPacket->longitude);
  }
  
  return messageType;
}

/**
 * @brief Validate a packet of any supported type
 * 
 * Performs comprehensive validation on a packet based on its message type.
 * Checks packet structure, field values, and internal consistency.
 * 
 * @param data Pointer to the packet data
 * @param dataSize Size of the packet data in bytes
 * @param verbose Whether to print detailed error messages to Serial
 * @return true if the packet is valid, false otherwise
 */
bool SensorSentinel_validate_packet(const void* data, size_t dataSize, bool verbose) {
  if (!data) {
    if (verbose) Serial.println("ERROR: Null packet pointer");
    return false;
  }
  
  // Need at least one byte for the message type
  if (dataSize < 1) {
    if (verbose) Serial.println("ERROR: Packet too small to contain message type");
    return false;
  }
  
  // Get the message type from the first byte
  uint8_t messageType = *((const uint8_t*)data);
  
  // Get the expected size for this message type
  size_t expectedSize = SensorSentinel_get_packet_size(messageType);
  
  // Check if it's a known message type
  if (expectedSize == 0) {
    if (verbose) Serial.printf("Unknown message type 0x%02X\n", messageType);
    return false;
  }
  
  // Check if the data size matches the expected size
  if (dataSize != expectedSize) {
    if (verbose) Serial.printf("ERROR: Incorrect packet size - expected %u bytes, got %u bytes\n", 
                  expectedSize, dataSize);
    return false;
  }
  
  // Validate based on message type
  switch (messageType) {
    case SensorSentinel_MSG_SENSOR: {
      const SensorSentinel_sensor_packet_t* packet = (const SensorSentinel_sensor_packet_t*)data;
      
      // Validate message type (should match what we extracted)
      if (packet->messageType != SensorSentinel_MSG_SENSOR) {
        if (verbose) Serial.println("ERROR: Message type field corrupted");
        return false;
      }
      
      // Validate node ID (non-zero)
      if (packet->nodeId == 0) {
        if (verbose) Serial.println("ERROR: Invalid node ID (zero)");
        return false;
      }
      
      // Validate battery level
      if (packet->batteryLevel > 100) {
        if (verbose) Serial.printf("ERROR: Invalid battery level: %u%%\n", packet->batteryLevel);
        return false;
      }
      
      // Log a warning for very low voltage that might indicate USB power
      if (packet->batteryVoltage < 2000 || packet->batteryVoltage > 4500) {
        Serial.printf("WARNING: Low battery voltage: %u mV (likely USB power)\n", packet->batteryVoltage);
      }
      
      // Validate analog readings (optional - depends on your hardware)
      // For example, if you know the ADC is 12-bit, values should be 0-4095
      for (int i = 0; i < 4; i++) {
        if (packet->pins.analog[i] > 4095) {
          if (verbose) Serial.printf("ERROR: Analog value A%d exceeds maximum (got %u)\n", 
                        i, packet->pins.analog[i]);
          return false;
        }
      }
      
      // All checks passed
      return true;
    }
    
    case SensorSentinel_MSG_GNSS: {
      const SensorSentinel_gnss_packet_t* packet = (const SensorSentinel_gnss_packet_t*)data;
      
      // Validate message type
      if (packet->messageType != SensorSentinel_MSG_GNSS) {
        if (verbose) Serial.println("ERROR: Message type field corrupted");
        return false;
      }
      
      // Validate node ID
      if (packet->nodeId == 0) {
        if (verbose) Serial.println("ERROR: Invalid node ID (zero)");
        return false;
      }
      
      // Validate battery level
      if (packet->batteryLevel > 100) {
        if (verbose) Serial.printf("ERROR: Invalid battery level: %u%%\n", packet->batteryLevel);
        return false;
      }
      
      // Log a warning for very low voltage that might indicate USB power
      if (packet->batteryVoltage < 2000 || packet->batteryVoltage > 4500) {
        Serial.printf("WARNING: Low battery voltage: %u mV (likely USB power)\n", packet->batteryVoltage);
      }
      
      
      // Validate latitude (-90 to +90 degrees)
      if (packet->latitude < -90.0 || packet->latitude > 90.0) {
        if (verbose) Serial.printf("ERROR: Invalid latitude: %.6f\n", packet->latitude);
        return false;
      }
      
      // Validate longitude (-180 to +180 degrees)
      if (packet->longitude < -180.0 || packet->longitude > 180.0) {
        if (verbose) Serial.printf("ERROR: Invalid longitude: %.6f\n", packet->longitude);
        return false;
      }
      
      // Validate speed (non-negative)
      if (packet->speed < 0.0) {
        if (verbose) Serial.printf("ERROR: Negative speed: %.1f km/h\n", packet->speed);
        return false;
      }
      
      // Validate course (0-359.99 degrees)
      if (packet->course < 0.0 || packet->course >= 360.0) {
        if (verbose) Serial.printf("ERROR: Invalid course: %.1f degrees\n", packet->course);
        return false;
      }
      
      // Validate HDOP (typically 0-100, higher values indicate poor precision)
      if (packet->hdop > 200) {  // Very high HDOP suggests bad data
        if (verbose) Serial.printf("ERROR: Unrealistic HDOP value: %.1f\n", packet->hdop / 10.0f);
        return false;
      }
      
      // All checks passed
      return true;
    }
    
    default:
      // This shouldn't happen (we already checked message type)
      if (verbose) Serial.printf("ERROR: Unhandled message type 0x%02X\n", messageType);
      return false;
  }
}

bool SensorSentinel_copy_packet(void* dest, size_t destSize, const void* src, bool verbose = false) {
  if (!dest || !src) {
    if (verbose) Serial.println("ERROR: Null pointer in copy operation");
    return false;
  }
  
  // Get the message type from the source packet
  uint8_t messageType = *((const uint8_t*)src);
  
  // Get the appropriate size based on message type
  size_t copySize = SensorSentinel_get_packet_size(messageType);
  
  if (copySize == 0) {
    if (verbose) Serial.printf("ERROR: Unknown packet type 0x%02X\n", messageType);
    return false;
  }
  
  if (destSize < copySize) {
    if (verbose) Serial.printf("ERROR: Destination buffer too small (need %u, have %u)\n", 
                             copySize, destSize);
    return false;
  }
  
  // Perform the copy
  memcpy(dest, src, copySize);
  
  if (verbose) {
    Serial.printf("INFO: Successfully copied packet type 0x%02X (%u bytes)\n", 
                 messageType, copySize);
  }
  
  return true;
}

/**
 * @brief Convert a packet to a JSON document
 * 
 * Takes a packet of any supported type and populates a JsonDocument object.
 * This allows for further modification of the JSON before serialization.
 * 
 * @param packet Pointer to the packet to convert
 * @param doc JsonDocument to populate
 * @param errorCode Optional pointer to store error code
 * @return true if conversion succeeded, false if failed
 */
bool SensorSentinel_packet_to_json_doc(const void* packet, JsonDocument& doc, 
                              SensorSentinel_json_error_t* errorCode) {
  // Initialize error code to success
  if (errorCode) *errorCode = SensorSentinel_JSON_SUCCESS;
  
  // Validate input parameters
  if (!packet) {
    if (errorCode) *errorCode = SensorSentinel_JSON_ERROR_NULL_PARAMS;
    return false;
  }
  
  // Get the message type from the packet
  uint8_t messageType = *((const uint8_t*)packet);
  
  // Clear any existing content in the document
  try {
    doc.clear();
  } catch (...) {
    // Handle exceptions from ArduinoJson operations
    if (errorCode) *errorCode = SensorSentinel_JSON_ERROR_SERIALIZATION;
    return false;
  }
  
  // Convert different packet types to JSON
  try {
    switch (messageType) {
      case SensorSentinel_MSG_SENSOR: {
        const SensorSentinel_sensor_packet_t* sensorPacket = (const SensorSentinel_sensor_packet_t*)packet;
        
        // Validate essential data
        if (sensorPacket->nodeId == 0) {
          if (errorCode) *errorCode = SensorSentinel_JSON_ERROR_INVALID_DATA;
          return false;
        }
        
        // Add common fields
        doc["type"] = "sensor";
        doc["nodeId"] = sensorPacket->nodeId;
        doc["counter"] = sensorPacket->messageCounter;
        doc["battery"] = sensorPacket->batteryLevel;
        doc["voltage"] = sensorPacket->batteryVoltage;
        doc["uptime"] = sensorPacket->uptime;
        
        // Add sensor-specific fields
        JsonArray analog = doc["analog"].to<JsonArray>();
        for (int i = 0; i < 4; i++) {
          analog.add(sensorPacket->pins.analog[i]);
        }
        doc["digital"] = sensorPacket->pins.boolean;
        break;
      }
      
      case SensorSentinel_MSG_GNSS: {
        const SensorSentinel_gnss_packet_t* gnssPacket = (const SensorSentinel_gnss_packet_t*)packet;
        
        // Validate essential data
        if (gnssPacket->nodeId == 0 ||
            gnssPacket->latitude < -90.0 || gnssPacket->latitude > 90.0 ||
            gnssPacket->longitude < -180.0 || gnssPacket->longitude > 180.0) {
          if (errorCode) *errorCode = SensorSentinel_JSON_ERROR_INVALID_DATA;
          return false;
        }
        
        // Add common fields
        doc["type"] = "gnss";
        doc["nodeId"] = gnssPacket->nodeId;
        doc["counter"] = gnssPacket->messageCounter;
        doc["battery"] = gnssPacket->batteryLevel;
        doc["voltage"] = gnssPacket->batteryVoltage;
        doc["uptime"] = gnssPacket->uptime;
        
        // Add GNSS-specific fields
        doc["latitude"] = gnssPacket->latitude;
        doc["longitude"] = gnssPacket->longitude;
        doc["speed"] = gnssPacket->speed;
        doc["course"] = gnssPacket->course;
        doc["hdop"] = (float)gnssPacket->hdop / 10.0;
        break;
      }
      
      default:
        // Unknown packet type
        if (errorCode) *errorCode = SensorSentinel_JSON_ERROR_UNKNOWN_TYPE;
        return false;
    }
  } catch (...) {
    // Handle any JSON document manipulation errors
    if (errorCode) *errorCode = SensorSentinel_JSON_ERROR_SERIALIZATION;
    return false;
  }
  
  return true;
}

/**
 * @brief Convert a packet to JSON string format
 * 
 * Takes a packet of any supported type and converts it to a JSON string.
 * This is a wrapper around SensorSentinel_packet_to_json_doc that handles serialization.
 * 
 * @param packet Pointer to the packet to convert
 * @param buffer Output buffer to store the JSON string
 * @param bufferSize Size of the output buffer in bytes
 * @param errorCode Optional pointer to store error code
 * @param prettyPrint Whether to format the JSON with indentation (default: false)
 * @return true if conversion succeeded, false if failed
 */
bool SensorSentinel_packet_to_json(const void* packet, char* buffer, size_t bufferSize, 
                          SensorSentinel_json_error_t* errorCode, 
                          bool prettyPrint) {
  // Initialize error code to success
  if (errorCode) *errorCode = SensorSentinel_JSON_SUCCESS;
  
  // Validate input parameters
  if (!packet || !buffer || bufferSize == 0) {
    if (errorCode) *errorCode = SensorSentinel_JSON_ERROR_NULL_PARAMS;
    
    // If buffer is valid, try to write an error message
    if (buffer && bufferSize > 0) {
      snprintf(buffer, bufferSize, "{\"error\":\"Invalid parameters\"}");
    }
    return false;
  }
  

  
  // Ensure the buffer always contains valid JSON by default (minimal valid JSON)
  if (bufferSize >= 3) {
    buffer[0] = '{';
    buffer[1] = '}';
    buffer[2] = '\0';
  } else if (bufferSize > 0) {
    buffer[0] = '\0';
  }
  
  // Create a JSON document
  JsonDocument doc;  // Adjust size based on your needs
  
  // Convert packet to JSON document
  SensorSentinel_json_error_t conversionError = SensorSentinel_JSON_SUCCESS;
  if (!SensorSentinel_packet_to_json_doc(packet, doc, &conversionError)) {
    // Propagate error code if caller requested it
    if (errorCode) *errorCode = conversionError;
    
    // Generate appropriate error message based on the specific error
    const char* errorMsg = "Conversion failed";
    char details[30] = {0};  // For additional error details
    
    switch (conversionError) {
      case SensorSentinel_JSON_ERROR_NULL_PARAMS:
        errorMsg = "Invalid parameters";
        break;
      case SensorSentinel_JSON_ERROR_SMALL_BUFFER:
        errorMsg = "JSON document capacity too small";
        break;
      case SensorSentinel_JSON_ERROR_INVALID_DATA:
        errorMsg = "Invalid packet data";
        break;
      case SensorSentinel_JSON_ERROR_UNKNOWN_TYPE: {
        errorMsg = "Unknown packet type";
        if (packet) {
          uint8_t messageType = *((const uint8_t*)packet);
          snprintf(details, sizeof(details), ": 0x%02X", messageType);
        }
        break;
      }
      case SensorSentinel_JSON_ERROR_SERIALIZATION:
        errorMsg = "JSON serialization error";
        break;
      default:
        break;
    }
    
    // Safely generate the error message JSON
    // Use a minimal JSON object with just the error field to ensure it fits
    if (details[0] != '\0') {
      snprintf(buffer, bufferSize, "{\"error\":\"%s%s\"}", errorMsg, details);
    } else {
      snprintf(buffer, bufferSize, "{\"error\":\"%s\"}", errorMsg);
    }
    
    return false;
  }
  
  // Pre-check if the buffer might be too small by measuring the JSON size
  size_t requiredSize = 0;
  try {
    requiredSize = prettyPrint ? measureJsonPretty(doc) : measureJson(doc);
  } catch (...) {
    if (errorCode) *errorCode = SensorSentinel_JSON_ERROR_SERIALIZATION;
    snprintf(buffer, bufferSize, "{\"error\":\"Failed to measure JSON size\"}");
    return false;
  }
  
  if (requiredSize >= bufferSize) {
    if (errorCode) *errorCode = SensorSentinel_JSON_ERROR_SMALL_BUFFER;
    // Provide detailed error with size information, but make sure it fits
    snprintf(buffer, bufferSize, "{\"error\":\"Buffer too small\",\"required\":%u}", 
             (unsigned int)requiredSize);
    return false;
  }
  
  // Serialize JSON to the output buffer
  size_t bytesWritten = 0;
  try {
    if (prettyPrint) {
      bytesWritten = serializeJsonPretty(doc, buffer, bufferSize);
    } else {
      bytesWritten = serializeJson(doc, buffer, bufferSize);
    }
  } catch (...) {
    // Handle any unexpected exceptions from ArduinoJson
    if (errorCode) *errorCode = SensorSentinel_JSON_ERROR_SERIALIZATION;
    snprintf(buffer, bufferSize, "{\"error\":\"JSON serialization failed\"}");
    return false;
  }
  
  // Double-check the serialization result
  if (bytesWritten == 0 || bytesWritten >= bufferSize) {
    if (errorCode) *errorCode = SensorSentinel_JSON_ERROR_SERIALIZATION;
    snprintf(buffer, bufferSize, "{\"error\":\"JSON serialization failed\"}");
    return false;
  }
  
  return true;
}

/**
 * @brief Print a packet as JSON to Serial
 */
bool SensorSentinel_print_packet_json(const void* packet, bool prettyPrint) {
  if (!packet) {
    Serial.println("ERROR: Null packet pointer in SensorSentinel_print_packet_json");
    return false;
  }
  
  // Create a buffer for the JSON string
  char jsonBuffer[512];  // Adjust size as needed
  
  // Convert the packet to JSON
  if (SensorSentinel_packet_to_json(packet, jsonBuffer, sizeof(jsonBuffer), nullptr, prettyPrint)) {
    // Print a header
    Serial.println("\nJSON:");
    
    // Print the JSON string
    Serial.println(jsonBuffer);
    
    return true;
  } else {
    Serial.println("ERROR: Failed to convert packet to JSON");
    return false;
  }
}