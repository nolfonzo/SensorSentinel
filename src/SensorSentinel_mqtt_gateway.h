/**
 * @file SensorSentinel_mqtt_gateway.h
 * @brief MQTT helper functions for Heltec boards
 */

#ifndef SensorSentinel_MQTT_GATEWAY_H
#define SensorSentinel_MQTT_GATEWAY_H

#include <Arduino.h>
#include <PubSubClient.h>  // MQTT client library
#include <ArduinoJson.h>   // JSON processing library
#include <WiFi.h>          // ESP32 WiFi library

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
 * @brief Add timestamp fields to a JSON document
 * 
 * Adds timestamp_ms (millis) and, if time is synchronized, also adds:
 * - timestamp (Unix epoch)
 * - time (formatted time string, if requested)
 * - time_iso (ISO 8601 format, if requested)
 * 
 * @param doc JSON document to add timestamp fields to
 * @param useFormattedTime Whether to include formatted time strings (default: false)
 * @return boolean True if synchronized time was available
 */
boolean SensorSentinel_mqtt_add_timestamp(JsonDocument& doc, bool useFormattedTime = false);

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
 * @brief Publish a JSON document to an MQTT topic
 * 
 * Serializes the JSON document and publishes it to the specified topic.
 * Automatically adds timestamp information.
 * 
 * @param topic The topic to publish to
 * @param doc The JSON document to publish
 * @param retained Whether the message should be retained by the broker (default: false)
 * @param useFormattedTime Whether to include formatted time in timestamps (default: false)
 * @return boolean True if published successfully
 */
boolean SensorSentinel_mqtt_publish_json(const char* topic, JsonDocument& doc, boolean retained = false, boolean useFormattedTime = false);

/**
 * @brief Publish device status to the status topic
 * 
 * Creates a detailed status message with device information, runtime metrics,
 * connection details, and publishes it to the status topic.
 * 
 * @param status Status message (e.g., "ok", "error", etc.)
 * @param retained Whether the message should be retained by the broker (default: true)
 * @return boolean True if published successfully
 */
boolean SensorSentinel_mqtt_publish_status(const char* status, boolean retained = true);

/**
 * @brief Publish "ok" status with default settings
 * 
 * Simplified version of SensorSentinel_mqtt_publish_status that uses "ok" as status
 * and sets the retained flag to true.
 * 
 * @return boolean True if published successfully
 */
boolean SensorSentinel_mqtt_publish_status();

/**
 * @brief Display MQTT status on the Heltec OLED display
 * 
 * Shows a compact status summary on the display with WiFi signal strength,
 * MQTT connection status, packet count, battery level, and uptime.
 * Also logs detailed information to the serial console.
 * 
 */
void SensorSentinel_mqtt_log_status();

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

#endif // SensorSentinel_MQTT_GATEWAY_H