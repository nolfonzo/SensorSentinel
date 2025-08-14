/**
 * @file SensorSentinel_mqtt_helper.cpp
 * @brief Implementation of MQTT helper functions for Heltec boards
 */

#include "heltec_unofficial_revised.h"
#include <WiFi.h>  // Include this explicitly for WiFiClient
#include "SensorSentinel_mqtt_helper.h"
#include "SensorSentinel_wifi_helper.h"

// Move this define to here
#ifndef MQTT_TOPIC
#define MQTT_TOPIC "lora/data"
#endif

// Magic constant replaced with a named constant
#define TIME_SYNC_EPOCH 1600000000  // Sept 2020, indicates time is synced

// WiFi and MQTT client instances
extern WiFiClient wifiClient;  // Declare external reference to WiFi client
PubSubClient mqttClient(wifiClient);  // Use the external WiFi client

// Define the config instance (not the struct)
MqttConfig mqttConfig = {
    .server = MQTT_SERVER,
    .port = MQTT_PORT,
    .user = MQTT_USER,
    .password = MQTT_PASSWORD,
    .connectionInterval = 5000,
    .socketTimeout = 10,  // Add comma here if you plan to add more fields
};

// Module variables
static String mqttClientId = "";
static unsigned long lastMqttConnectionAttempt = 0;
static unsigned long lastPublishTime = 0;
static uint32_t reconnectCounter = 0;
static uint32_t publishCount = 0;

// Replace the getMqttStateString function with:

static const struct { int code; const char* desc; } MQTT_STATES[] = {
    {-4, "Connection Timeout"}, {-3, "Connection Lost"}, {-2, "Connect Failed"},
    {-1, "Disconnected"}, {0, "Connected"}, {1, "Bad Protocol"},
    {2, "Bad Client ID"}, {3, "Unavailable"}, {4, "Bad Credentials"}, {5, "Unauthorized"}
};

const char* getMqttStateString(int state) {
    for (const auto& s : MQTT_STATES) {
        if (s.code == state) return s.desc;
    }
    return "Unknown Error";
}

/**
 * @brief Sync time with default parameters
 */
boolean SensorSentinel_mqtt_sync_time_default() {
  // Default NTP servers and timezone
  #ifdef TIMEZONE_OFFSET
  long timezone = TIMEZONE_OFFSET;
  #else
  long timezone = 0; // UTC
  #endif
  
  return SensorSentinel_mqtt_sync_time(timezone, 0, 
                        "pool.ntp.org", "time.nist.gov", "time.google.com");
}

/**
 * @brief Synchronize time via NTP with custom parameters
 */
boolean SensorSentinel_mqtt_sync_time(
    long timezone,
    int daylightOffset,
    const char* ntpServer1,
    const char* ntpServer2,
    const char* ntpServer3) {
    
    configTime(timezone, daylightOffset, ntpServer1, ntpServer2, ntpServer3);
    
    time_t now = time(nullptr);
    int attempts = 0;
    while (now < TIME_SYNC_EPOCH && attempts < 20) {
        delay(500);
        now = time(nullptr);
        attempts++;
    }
    
    return now >= TIME_SYNC_EPOCH;
}

/**
 * @brief Get a unique client ID for MQTT based on ESP32's MAC address
 */
String SensorSentinel_mqtt_get_client_id() {
    uint32_t nodeId = SensorSentinel_generate_node_id();
    return "SensorSentinel-" + String(nodeId, HEX);  // e.g., "SensorSentinel-12345678"
}

/**
 * @brief Initialize the MQTT client with server settings
 */
boolean SensorSentinel_mqtt_init() {
  Serial.println("Initializing MQTT client...");
  
  // Validate MQTT server settings
  if (strlen(mqttConfig.server) == 0) {
    Serial.println("ERROR: MQTT server address is empty");
    return false;
  }
  
  if (mqttConfig.port <= 0 || mqttConfig.port > 65535) {
    Serial.printf("ERROR: Invalid MQTT port: %d\n", mqttConfig.port);
    return false;
  }
  
  // Configure the MQTT client with server and port
  mqttClient.setServer(mqttConfig.server, mqttConfig.port);
  
  // Test client ID generation
  mqttClientId = SensorSentinel_mqtt_get_client_id();
  
  // Log the MQTT configuration
  Serial.printf("\nMQTT Server: %s:%d\n", mqttConfig.server, mqttConfig.port);
  Serial.printf("\nMQTT Buffer Size: %u bytes\n", mqttClient.getBufferSize());
  
  return true;
}

/**
 * @brief Connect to the MQTT broker
 */
boolean SensorSentinel_mqtt_connect() {
    if (!SensorSentinel_wifi_connected()) {
        return false;
    }
    
    if (mqttClient.connected()) {
        return true;
    }
    
    // Rate limiting
    unsigned long now = millis();
    if (now - lastMqttConnectionAttempt < mqttConfig.connectionInterval) {
        return false;
    }
    
    lastMqttConnectionAttempt = now;
    String clientId = SensorSentinel_mqtt_get_client_id();
    mqttClient.setSocketTimeout(mqttConfig.socketTimeout);
    
    boolean success = strlen(mqttConfig.user) > 0 ?
        mqttClient.connect(clientId.c_str(), mqttConfig.user, mqttConfig.password) :
        mqttClient.connect(clientId.c_str());
    
    reconnectCounter = success ? 0 : reconnectCounter + 1;
    
    if (success) {
        Serial.println("MQTT connected successfully");
    } else {
        Serial.printf("MQTT connection failed: %s\n", getMqttStateString(mqttClient.state()));
    }
    
    return success;
}

/**
 * @brief Forward a packet to the MQTT broker
 * @param data Pointer to the data to be sent
 * @param length Length of the data
 * @param rssi RSSI value for the packet
 * @param snr SNR value for the packet
 * @return Status of the forwarding operation
 */
MqttForwardStatus SensorSentinel_mqtt_forward_packet(uint8_t *data, size_t length, float rssi, float snr) {
    if (!SensorSentinel_validate_packet(data, length)) return MQTT_INVALID_PACKET;
    if (!mqttClient.connected()) return MQTT_NOT_CONNECTED;
    return mqttClient.publish(MQTT_TOPIC, data, length, false) ? MQTT_SUCCESS : MQTT_PUBLISH_FAILED;
}

/**
 * @brief Convert MQTT forward status to string
 */
const char* SensorSentinel_mqtt_status_to_string(MqttForwardStatus status) {
    switch(status) {
        case MQTT_SUCCESS:
            return "Success";
        case MQTT_NOT_CONNECTED:
            return "Not Connected";
        case MQTT_PUBLISH_FAILED:
            return "Publish Failed";
        case MQTT_INVALID_PACKET:
            return "Invalid Packet";
        default:
            return "Unknown Status";
    }
}

/**
 * @brief Setup MQTT connection
 */
boolean SensorSentinel_mqtt_setup(bool enableLogging) {
    if (!SensorSentinel_wifi_connected()) {
        Serial.println("Cannot setup MQTT - WiFi not connected");
        return false;
    }

    if (!SensorSentinel_mqtt_init()) {
        Serial.println("MQTT initialization failed");
        return false;
    }

    return true;
}

/**
 * @brief Maintain MQTT connection and handle keepalive
 */
boolean SensorSentinel_mqtt_maintain() {
    if (!mqttClient.connected()) {
        return SensorSentinel_mqtt_connect();
    }
    mqttClient.loop();
    return true;
}