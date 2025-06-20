/**
 * @file SensorSentinel_Packet_Helper.h
 * @brief Packet structure definitions for Heltec boards
 * 
 * Defines standardized packet formats for sending sensor data 
 * and GNSS/location data from Heltec boards to gateways or other devices.
 * 
 * The packet structures are separated into two types:
 * 1. Sensor packets - containing basic sensor readings and device status
 * 2. GNSS packets - containing location and movement data
 * 
 * This separation allows for different transmission frequencies and
 * optimizes bandwidth usage, particularly for LoRa applications.
 */

#ifndef SensorSentinel_PACKET_HELPER_H
#define SensorSentinel_PACKET_HELPER_H

#include <Arduino.h>
#include "SensorSentinel_pins_helper.h"  // For SensorSentinel_pin_readings_t structure

/**
 * @brief Message types for different packet categories
 */
#define SensorSentinel_MSG_SENSOR        0x01  // Basic sensor data packet
#define SensorSentinel_MSG_GNSS         0x02  // GNSS location data packet

/**
 * @brief Basic sensor packet structure
 * 
 * Contains sensor readings and device status information,
 * but does not include GNSS/location data.
 */
typedef struct {
  // Header information
  uint8_t messageType;         // 
  uint32_t nodeId;             // Unique node identifier (from MAC address)
  uint32_t messageCounter;     // Sequence number
  uint32_t uptime;             // uptime in seconds
  uint8_t batteryLevel;        // Battery level (0-100%)
  uint16_t batteryVoltage;     // Battery voltage in millivolts

  // Sensor data
  SensorSentinel_pin_readings_t pins;  // All pin readings in a standard format
  
  // Reserved for future expansion
  uint8_t reserved[2];
} __attribute__((packed)) SensorSentinel_sensor_packet_t;

/**
 * @brief GNSS location packet structure
 * 
 * Contains geographic position and movement data.
 * Sent separately from sensor data and typically at a lower frequency.
 */
typedef struct {
  // Header information
  uint8_t messageType;         // Always SensorSentinel_MSG_GNSS (0x02)
  uint32_t nodeId;             // Unique node identifier (from MAC address)
  uint32_t messageCounter;     // Sequence number
  uint32_t uptime;             // uptime in seconds
  uint8_t batteryLevel;        // Battery level (0-100%)
  uint16_t batteryVoltage;     // Battery voltage in millivolts

  // GNSS/GPS data
  float latitude;              // Degrees
  float longitude;             // Degrees  
  float speed;                 // Speed in km/h
  uint8_t hdop;                // Horizontal dilution of precision * 10 (lower is better)
  float course;                // Course/heading in degrees (0-359.99)
  
  // Reserved for future expansion
  uint8_t reserved[2];
} __attribute__((packed)) SensorSentinel_gnss_packet_t;

/**
 * @brief Union for handling different packet types
 *
 * Allows working with both packet types through a common interface
 * while maintaining proper memory layout.
 */
typedef union {
  struct {                      // Common header fields for type detection
    uint8_t messageType;
    uint32_t nodeId;
    uint32_t messageCounter;
  } header;
  SensorSentinel_sensor_packet_t sensor;
  SensorSentinel_gnss_packet_t gnss;
} SensorSentinel_packet_t;

/*/**
 * @brief Get a unique node ID based on the ESP32's MAC address
 * 
 * @return 32-bit unique identifier derived from the MAC address
 */
uint32_t SensorSentinel_get_node_id();

/**
 * @brief Initialize a basic sensor packet with device information
 * 
 * Populates a packet with node ID, uptime, pin readings and battery level.
 * 
 * @param packet Pointer to sensor packet structure to initialize
 * @param counter Message sequence counter value
 * @return true if initialization was successful, false otherwise
 */
bool SensorSentinel_init_sensor_packet(SensorSentinel_sensor_packet_t* packet, uint32_t counter);

/**
 * @brief Initialize a GNSS packet with device information
 * 
 * Populates a packet with node ID, uptime, and battery level.
 * GNSS fields must be populated separately after a valid position fix.
 * 
 * @param packet Pointer to GNSS packet structure to initialize
 * @param counter Message sequence counter value
 * @return true if initialization was successful, false otherwise
 */
bool SensorSentinel_init_gnss_packet(SensorSentinel_gnss_packet_t* packet, uint32_t counter);

/**
 * @brief Get the size of a packet based on its message type
 * 
 * Returns the appropriate size in bytes for a given message type.
 * 
 * @param messageType The type of message (SensorSentinel_MSG_BASIC or SensorSentinel_MSG_GNSS)
 * @return The size of the packet in bytes
 */
size_t SensorSentinel_get_packet_size(uint8_t messageType);

/**
 * @brief Print packet information to Serial for debugging
 * 
 * Outputs a human-readable representation of any packet type based on the
 * message type field in the packet. Handles both sensor and GNSS packets.
 * 
 * @param packet Pointer to the packet buffer (any type)
 * @param showAll Whether to show all fields including reserved bytes
 * @return true if the packet was successfully printed, false otherwise
 */
bool SensorSentinel_print_packet_info(const void* packet, bool showAll = false);

/**
 * @brief Parse a received packet buffer into the appropriate packet structure
 * 
 * Converts raw bytes from radio/serial into a structured packet,
 * validates the data, and returns success/failure.
 * 
 * @param buffer Raw buffer containing packet data
 * @param length Length of the buffer in bytes
 * @param packet Pointer to packet union to populate
 * @return true if parsing was successful, false if packet is invalid
 */
bool SensorSentinel_parse_packet(uint8_t* buffer, size_t length, SensorSentinel_packet_t* packet);

/**
 * @brief Validate a sensor packet structure for consistency
 * 
 * Checks that a packet has valid values and is internally consistent.
 * 
 * @param packet Pointer to sensor packet structure to validate
 * @return true if the packet is valid, false otherwise
 */
bool SensorSentinel_validate_sensor_packet(const SensorSentinel_sensor_packet_t* packet);

/**
 * @brief Validate a GNSS packet structure for consistency
 * 
 * Checks that a packet has valid values and is internally consistent.
 * 
 * @param packet Pointer to GNSS packet structure to validate
 * @return true if the packet is valid, false otherwise
 */
bool SensorSentinel_validate_gnss_packet(const SensorSentinel_gnss_packet_t* packet);

/*/**
 * @brief Validate a packet of any supported type
 * 
 * Examines the message type and calls the appropriate type-specific
 * validation function. Simplifies validation of packets when the
 * exact type is not known at compile time.
 * 
 * @param data Pointer to the packet data
 * @param dataSize Size of the packet data in bytes
 * @param verbose Whether to print detailed error messages to Serial
 * @return true if the packet is valid, false otherwise
 */
bool SensorSentinel_validate_packet(const void* data, size_t dataSize, bool verbose = false);

#endif // SensorSentinel_PACKET__HELPER_H