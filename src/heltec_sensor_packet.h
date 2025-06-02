/**
 * @file Heltec_Sensor_Packet.h
 * @brief Packet structure definitions for Heltec boards
 * 
 * Defines a standardized packet format for sending sensor data 
 * from Heltec boards to gateways or other devices.
 * 
 * The packet structure supports both basic sensor data and
 * optional GNSS (GPS) data when available.
 */

#ifndef HELTEC_SENSOR_PACKET_H
#define HELTEC_SENSOR_PACKET_H

#include <Arduino.h>
#include "Heltec_Pins.h"  // For heltec_pin_readings_t structure

// Include ArduinoJson if available (for JSON conversion)
#if __has_include(<ArduinoJson.h>)
  #include <ArduinoJson.h>
  #define HELTEC_SENSOR_JSON_SUPPORT
#endif

/**
 * @brief Message types for sensor packets
 */
#define HELTEC_MSG_BASIC    0x01  // Basic status (no GNSS data)
#define HELTEC_MSG_GNSS     0x02  // Status with GNSS/GPS data

/**
 * @brief Comprehensive sensor packet structure
 * 
 * This packet is designed to be sent from nodes to the gateway.
 * The structure includes a variable section - when messageType is BASIC,
 * the GNSS fields are not included in transmission to save bandwidth.
 */
typedef struct {
  // Header information
  uint8_t messageType;         // Type of message (BASIC or GNSS)
  uint8_t packetSize;          // Size of the packet in bytes
  uint32_t nodeId;             // Unique node identifier (from MAC address)
  uint32_t messageCounter;     // Sequence number
  uint32_t timestamp;          // Unix timestamp (seconds since epoch) or uptime in seconds
  
  // Pin data
  heltec_pin_readings_t pins;  // All pin readings in a standard format
  
  // System status
  uint8_t batteryPercent;      // 0-100%
  uint16_t batteryMv;          // Battery voltage in millivolts
  int8_t rssi;                 // RSSI value in dBm
  uint8_t snr;                 // Signal-to-noise ratio
  
  // The fields below are only valid if messageType = HELTEC_MSG_GNSS
  
  // GNSS/GPS data
  float latitude;              // Degrees
  float longitude;             // Degrees  
  int16_t altitude;            // Meters
  uint8_t satellites;          // Number of satellites
  uint8_t hdop;                // Horizontal dilution of precision * 10 (lower is better)
  
  // Reserved for future use
  uint8_t reserved[2];
} __attribute__((packed)) heltec_sensor_packet_t;

/**
 * @brief Get a unique node ID based on the ESP32's MAC address
 * 
 * @return 32-bit unique identifier derived from the MAC address
 */
uint32_t heltec_get_node_id();

/**
 * @brief Initialize a sensor packet with basic information
 * 
 * Populates a packet with node ID, timestamp, pin readings and battery level.
 * Also sets the appropriate packet size based on whether GNSS data will be included.
 * 
 * @param packet Pointer to packet structure to initialize
 * @param includeGnss Whether to include GNSS data (affects messageType)
 * @param counter Message sequence counter value
 */
void heltec_init_sensor_packet(heltec_sensor_packet_t* packet, bool includeGnss, uint32_t counter);

/**
 * @brief Get the size of a packet based on its message type
 * 
 * Returns the appropriate size in bytes for a given message type.
 * BASIC packets exclude the GNSS fields to save bandwidth.
 * 
 * @param messageType The type of message (HELTEC_MSG_BASIC or HELTEC_MSG_GNSS)
 * @return The size of the packet in bytes
 */
size_t heltec_get_packet_size(uint8_t messageType);

/**
 * @brief Print packet information to Serial for debugging
 * 
 * Outputs a human-readable representation of the packet contents.
 * 
 * @param packet Pointer to packet structure to print
 * @param verbose Whether to show detailed information including all pin values
 */
void heltec_print_packet_info(const heltec_sensor_packet_t* packet, bool verbose = false);

/**
 * @brief Parse a received packet buffer into a packet structure
 * 
 * Converts raw bytes from radio/serial into a structured packet,
 * validates the data, and returns success/failure.
 * 
 * @param buffer Raw buffer containing packet data
 * @param length Length of the buffer in bytes
 * @param packet Pointer to packet structure to populate
 * @return true if parsing was successful, false if packet is invalid
 */
bool heltec_parse_packet(uint8_t* buffer, size_t length, heltec_sensor_packet_t* packet);

/**
 * @brief Validate a packet structure for consistency
 * 
 * Checks that a packet has valid values and is internally consistent.
 * 
 * @param packet Pointer to packet structure to validate
 * @return true if the packet is valid, false otherwise
 */
bool heltec_validate_packet(const heltec_sensor_packet_t* packet);

/**
 * @brief Copy packet data from source to destination
 * 
 * Copies only the relevant portions of the packet based on message type.
 * This ensures that unused fields in BASIC packets aren't copied unnecessarily.
 * 
 * @param dest Destination packet pointer
 * @param src Source packet pointer
 */
void heltec_copy_packet(heltec_sensor_packet_t* dest, const heltec_sensor_packet_t* src);

#ifdef HELTEC_SENSOR_JSON_SUPPORT
/**
 * @brief Convert packet to a complete JSON string
 * 
 * This is a convenience method that creates a JSON document, populates it,
 * and returns the serialized string.
 * 
 * @param packet Pointer to packet structure to convert
 * @param pretty Whether to format the JSON with indentation and whitespace
 * @return String containing the JSON representation of the packet
 */
String heltec_packet_to_json(const heltec_sensor_packet_t* packet, bool pretty = false);

/**
 * @brief Populate an existing JsonDocument with packet data
 * 
 * This method allows you to use your own JsonDocument (which may be part of a
 * larger document) and have it populated with the packet data. This is more
 * efficient when you want to add additional fields or when the JSON document
 * is being constructed incrementally.
 * 
 * @param packet Pointer to packet structure to convert
 * @param doc Reference to a JsonDocument to populate
 * @return true if the document was successfully populated, false otherwise
 */
bool heltec_packet_to_json_doc(const heltec_sensor_packet_t* packet, JsonDocument& doc);
#endif // HELTEC_SENSOR_JSON_SUPPORT

#endif // HELTEC_SENSOR_PACKET_H