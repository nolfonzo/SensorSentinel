/**
 * @file SensorSentinel_Sensor_Packet.cpp
 * @brief Implementation of packet handling functions for Heltec boards
 */

#include "SensorSentinel_packet_helper.h"
#include "heltec_unofficial_revised.h"
#include <string.h> // For memcpy

// Get a unique node ID based on ESP32's MAC address
uint32_t SensorSentinel_generate_node_id()
{
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
  for (int i = 0; i < 17; i = i + 8)
  {
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
bool SensorSentinel_init_sensor_packet(SensorSentinel_sensor_packet_t *packet, uint32_t counter)
{
  if (!packet)
    return false;

  // Clear the structure first
  memset(packet, 0, sizeof(SensorSentinel_sensor_packet_t));

  // Set message type
  packet->messageType = SensorSentinel_MSG_SENSOR; // Using new constant name

  // Set basic information
  packet->nodeId = SensorSentinel_generate_node_id(); // Updated function name
  packet->messageCounter = counter;
  packet->uptime = millis() / 1000; // Seconds since boot

  // Get battery information
  // float batteryVolts = heltec_vbat();
  float batteryVolts = 0.0f; // TODO, the battery code kills the display of the Tacker
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
bool SensorSentinel_init_gnss_packet(SensorSentinel_gnss_packet_t *packet, uint32_t counter)
{
  if (!packet)
    return false;

  // Clear the structure first
  memset(packet, 0, sizeof(SensorSentinel_gnss_packet_t));

  // Set message type
  packet->messageType = SensorSentinel_MSG_GNSS;

  // Set basic information
  packet->nodeId = SensorSentinel_generate_node_id(); // Updated function name
  packet->messageCounter = counter;
  packet->uptime = millis() / 1000; // Seconds since boot

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

  if (gps.location.isValid())
  {
    packet->latitude = gps.location.lat();
    packet->longitude = gps.location.lng();
    packet->speed = gps.speed.kmph();
    packet->course = gps.course.deg();
    packet->hdop = (uint8_t)(gps.hdop.hdop() * 10); // Store HDOP as fixed point
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
size_t SensorSentinel_get_packet_size(uint8_t messageType)
{
  switch (messageType)
  {
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
 * @return true if the packet was recognized and printed, false otherwise
 */
bool SensorSentinel_print_packet_info(const void *packet, size_t length)
{

  // Get the message type from the first byte
  uint8_t messageType = *((uint8_t *)packet);

  // Print common header information
  Serial.println("------ Packet Information ------");

  // Handle different packet types
  switch (messageType)
  {
  case SensorSentinel_MSG_SENSOR:
  {
    const SensorSentinel_sensor_packet_t *sensorPacket = (const SensorSentinel_sensor_packet_t *)packet;

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
    for (int i = 0; i < 4; i++)
    {
      Serial.printf("  A%d: %u\n", i, sensorPacket->pins.analog[i]);
    }

    // Print digital pin readings
    Serial.println("\nDigital Readings:");
    for (int i = 0; i < 8; i++)
    {
      bool pinState = (sensorPacket->pins.boolean >> i) & 0x01;
      Serial.printf("  D%d: %d\n", i, pinState);
    }

    // Print reserved bytes
    Serial.println("\nReserved Bytes:");
    for (int i = 0; i < sizeof(sensorPacket->reserved); i++)
    {
      Serial.printf("  [%d]: 0x%02X\n", i, sensorPacket->reserved[i]);
    }

    return true;
  }

  case SensorSentinel_MSG_GNSS:
  {
    const SensorSentinel_gnss_packet_t *gnssPacket = (const SensorSentinel_gnss_packet_t *)packet;

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

    // Print reserved bytes
    Serial.println("\nReserved Bytes:");
    for (int i = 0; i < sizeof(gnssPacket->reserved); i++)
    {
      Serial.printf("  [%d]: 0x%02X\n", i, gnssPacket->reserved[i]);
    }
    return true;
  }

  default:
    Serial.printf("Unknown packet type: 0x%02X\n", messageType);
    return false;
  }

  Serial.printf("\n\nRaw data (%u bytes): ", length);
  const uint8_t* packetData = static_cast<const uint8_t*>(packet);
  for (size_t i = 0; i < length; i++)
  {
    Serial.printf("%02X", packetData[i]);
  }
  Serial.println();
}

/**
 * @brief Validate a packet of any supported type
 *
 * Performs comprehensive validation on a packet based on its message type.
 * Checks packet structure, field values, and internal consistency.
 *
 * @param data Pointer to the packet data
 * @param length Size of the packet data in bytes
 * @return true if the packet is valid, false otherwise
 */
bool SensorSentinel_validate_packet(const void *data, size_t length)
{ 
// Check packet size
  if (length > MAX_LORA_PACKET_SIZE)
  {
    Serial.println("Packet too large!");
    Serial.printf("\nSize: %u bytes (max %u)\n", length, MAX_LORA_PACKET_SIZE);
    return false;
  }

  if (!data)
  {
    Serial.println("ERROR: Null packet pointer");
    return false;
  }

  // Need at least one byte for the message type
  if (length < 1)
  {
    Serial.println("ERROR: Packet length zero");
    return false;
  }

  // Get the message type from the first byte
  uint8_t messageType = *((const uint8_t *)data);

  // Get the expected size for this message type
  size_t expectedSize = SensorSentinel_get_packet_size(messageType);

  // Check if it's a known message type
  if (expectedSize == 0)
  {
    Serial.printf("Unknown message type 0x%02X\n", messageType);
  }

  // Check if the data size matches the expected size
  if (length != expectedSize)
  {
      Serial.printf("ERROR: Incorrect packet size - expected %u bytes, got %u bytes\n",
                    expectedSize, length);
    return false;
  }

  // All checks passed
  return true;

}

bool SensorSentinel_copy_packet(void *dest, size_t destSize, const void *src)
{
  if (!dest || !src)
  {
    Serial.println("ERROR: Null pointer in copy operation");
    return false;
  }

  // Get the message type from the source packet
  uint8_t messageType = *((const uint8_t *)src);

  // Get the appropriate size based on message type
  size_t copySize = SensorSentinel_get_packet_size(messageType);

  if (copySize == 0)
  {
    Serial.printf("ERROR: Unknown packet type 0x%02X\n", messageType);
    return false;
  }

  if (destSize < copySize)
  {
    Serial.printf("ERROR: Destination buffer too small (need %u, have %u)\n",
                    copySize, destSize);
    return false;
  }

  // Perform the copy
  memcpy(dest, src, copySize);

  Serial.printf("INFO: Successfully copied packet type 0x%02X (%u bytes)\n",
                  messageType, copySize);

  return true;
}

const char* SensorSentinel_message_type_to_string(uint8_t messageType) {
    switch(messageType) {
        case SensorSentinel_MSG_SENSOR:
            return "Sensor";
        case SensorSentinel_MSG_GNSS:
            return "GNSS";
        default:
            return "Unknown";
    }
}

uint32_t SensorSentinel_get_message_counter(const void* data) {
    const SensorSentinel_packet_t* packet = (const SensorSentinel_packet_t*)data;
    uint8_t messageType = packet->sensor.messageType;  // Both packet types have messageType at same offset

    if (messageType == SensorSentinel_MSG_SENSOR) {
        return packet->sensor.messageCounter;
    } else if (messageType == SensorSentinel_MSG_GNSS) {
        return packet->gnss.messageCounter;
    }
    return 0;
}

uint32_t SensorSentinel_extract_node_id_from_packet(const void* data) {
    const SensorSentinel_packet_t* packet = (const SensorSentinel_packet_t*)data;
    uint8_t messageType = packet->sensor.messageType;  // Both packet types have messageType at same offset

    if (messageType == SensorSentinel_MSG_SENSOR) {
        return packet->sensor.nodeId;
    } else if (messageType == SensorSentinel_MSG_GNSS) {
        return packet->gnss.nodeId;
    }
    return 0;
}

void SensorSentinel_print_invalid_packet(const uint8_t* data, size_t length) {
    Serial.println("Invalid packet contents:");
    
    // Print hex values
    Serial.print("HEX: ");
    for(size_t i = 0; i < length; i++) {
        if(data[i] < 0x10) Serial.print("0");
        Serial.print(data[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
    
    // Print ASCII representation
    Serial.print("ASCII: ");
    for(size_t i = 0; i < length; i++) {
        if(isprint(data[i])) {
            Serial.print((char)data[i]);
        } else {
            Serial.print(".");
        }
    }
    Serial.println("\n---------------------------");
}