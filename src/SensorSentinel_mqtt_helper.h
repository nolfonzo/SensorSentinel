/**
 * @file SensorSentinel_mqtt_helper.h
 * @brief MQTT helper functions for Heltec boards
 */

#ifndef SensorSentinel_MQTT_HELPER_H
#define SensorSentinel_MQTT_HELPER_H

#include <Arduino.h>
#include <PubSubClient.h>  // MQTT client library
#include <WiFi.h>          // ESP32 WiFi library
#include "SensorSentinel_packet_helper.h"

/**
 * @brief Status codes for MQTT forwarding operations
 */
enum MqttForwardStatus {
    MQTT_SUCCESS,      ///< Packet successfully forwarded
    MQTT_NOT_CONNECTED,///< MQTT client not connected
    MQTT_PUBLISH_FAILED,///< Publish operation failed
    MQTT_INVALID_PACKET ///< Packet validation failed
};

/**
 * @brief Synchronize time via NTP with custom parameters
 * 
 * @param timezone Timezone offset in seconds (e.g., 3600 for UTC+1)
 * @param daylightOffset Daylight saving time offset in seconds
 * @param ntpServer1 Primary NTP server address
 * @param ntpServer2 Secondary NTP server address 
 * @param ntpServer3 Tertiary NTP server address
 * @return boolean True if time was synchronized successfully
 */
boolean SensorSentinel_mqtt_sync_time(long timezone, int daylightOffset,
                      const char* ntpServer1, const char* ntpServer2, 
                      const char* ntpServer3);

/**
 * @brief Synchronize time via NTP with default parameters
 * 
 * Uses standard NTP servers and timezone from TIMEZONE_OFFSET if defined
 * 
 * @return boolean True if time was synchronized successfully
 */
boolean SensorSentinel_mqtt_sync_time_default();

/**
 * @brief Generate a unique MQTT client ID based on MAC address
 * 
 * Creates a client ID in format "HeltecGW-XXXXXX" where XXXXXX is derived
 * from the MAC address to ensure uniqueness across devices
 * 
 * @return String The generated client ID
 */
String SensorSentinel_mqtt_get_client_id();

/**
 * @brief Initialize the MQTT client with broker settings
 * 
 * Sets up the MQTT client with the server and port specified in
 * the global variables. Should be called before connecting.
 * 
 * @return boolean True if initialization was successful
 */
boolean SensorSentinel_mqtt_init();

/**
 * @brief Connect to the MQTT broker
 * 
 * Attempts to establish a connection to the MQTT broker using the
 * configured settings. Will use credentials if provided.
 * 
 * @return boolean True if connected successfully
 */
boolean SensorSentinel_mqtt_connect();

/**
 * @brief Set up MQTT with optional time synchronization
 * 
 * Complete setup process for MQTT: initialize, sync time if requested,
 * and establish connection to the broker.
 * 
 * @param syncTimeOnConnect Whether to synchronize time during setup (default: true)
 * @return boolean True if setup was successful
 */
boolean SensorSentinel_mqtt_setup(boolean syncTimeOnConnect = true);

/**
 * @brief Maintain MQTT connection
 * 
 * Should be called regularly in the main loop to ensure MQTT connection
 * remains active. Handles reconnection if needed.
 * 
 * @return boolean True if connected, false if disconnected
 */
boolean SensorSentinel_mqtt_maintain();

/**
 * @brief Publish a string message to an MQTT topic
 * 
 * @param topic The topic to publish to
 * @param payload The message content
 * @param retained Whether the message should be retained by the broker (default: false)
 * @return boolean True if published successfully
 */
boolean SensorSentinel_mqtt_publish(const char* topic, const char* payload, boolean retained = false);

/**
 * @brief Get reference to the MQTT client object
 * 
 * Provides direct access to the PubSubClient instance for advanced usage.
 * 
 * @return PubSubClient& Reference to the MQTT client
 */
PubSubClient& SensorSentinel_mqtt_get_client();

/**
 * @brief Calculate WiFi signal quality percentage from RSSI
 * 
 * Converts RSSI (dBm) to a quality percentage (0-100%)
 * -50dBm or better = 100%
 * -100dBm or worse = 0%
 * 
 * @return int Signal quality percentage (0-100)
 */
int SensorSentinel_wifi_quality();

/**
 * @brief Convert MQTT status to human-readable string
 * @param status The MQTT forward status code
 * @return const char* String representation of the status
 */
const char* SensorSentinel_mqtt_status_to_string(MqttForwardStatus status);

/**
 * @brief Forward a packet to MQTT broker
 * @param data Pointer to packet data
 * @param length Length of packet data
 * @param rssi Signal strength of received packet
 * @param snr Signal-to-noise ratio of received packet
 * @return MqttForwardStatus Status of the forward operation
 */
MqttForwardStatus SensorSentinel_mqtt_forward_packet(uint8_t *data, size_t length, float rssi, float snr);

struct MqttConfig {
    const char* server;
    int port;
    const char* user;
    const char* password;
    unsigned int connectionInterval;
    unsigned int socketTimeout;
};

extern MqttConfig mqttConfig;

#ifndef MQTT_TOPIC
#define MQTT_TOPIC "lora/data"  // Default topic if not defined in platformio.ini
#endif

#endif // SensorSentinel_MQTT_HELPER_H