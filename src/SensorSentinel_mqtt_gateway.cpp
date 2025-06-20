/**
 * @file SensorSentinel_mqtt_gateway.cpp
 * @brief Implementation of MQTT helper functions for Heltec boards
 */

#include "heltec_unofficial_revised.h"
#include <WiFi.h>  // Include this explicitly for WiFiClient
#include "SensorSentinel_mqtt_gateway.h"
#include "SensorSentinel_wifi_helper.h"

// Module variables
static PubSubClient mqttClient(wifiClient);
static String mqttClientId = "";
static unsigned long lastMqttConnectionAttempt = 0;
static unsigned long lastPublishTime = 0;
static uint32_t reconnectCounter = 0;
static uint32_t publishCount = 0;
static unsigned int mqttConnectionInterval = 5000; // 5 seconds between connection attempts

// Define MQTT configuration variables using platformio.ini values
#ifndef MQTT_USER
#define MQTT_USER ""  // Default to empty if not defined
#endif

#ifndef MQTT_PASSWORD
#define MQTT_PASSWORD ""  // Default to empty if not defined
#endif

// Define the actual global variables required by the MQTT implementation
const char* mqtt_server = MQTT_SERVER;
const int mqtt_port = MQTT_PORT;  
const char* mqtt_user = MQTT_USER;
const char* mqtt_password = MQTT_PASSWORD;

// Magic constant replaced with a named constant
#define TIME_SYNC_EPOCH 1600000000  // Sept 2020, indicates time is synced

// Template function for min
template <typename T>
T min(T a, T b) {
  return (a < b) ? a : b;
}

// Calculate WiFi signal quality from RSSI
int SensorSentinel_wifi_quality() {
  int rssi = SensorSentinel_wifi_rssi();
  if (rssi <= -100) {
    return 0;
  } else if (rssi >= -50) {
    return 100;
  } else {
    return 2 * (rssi + 100);
  }
}

// MQTT state error strings for better diagnostics
const char* getMqttStateString(int state) {
  switch (state) {
    case -4: return "MQTT_CONNECTION_TIMEOUT";
    case -3: return "MQTT_CONNECTION_LOST";
    case -2: return "MQTT_CONNECT_FAILED";
    case -1: return "MQTT_DISCONNECTED";
    case 0: return "MQTT_CONNECTED";
    case 1: return "MQTT_CONNECT_BAD_PROTOCOL";
    case 2: return "MQTT_CONNECT_BAD_CLIENT_ID";
    case 3: return "MQTT_CONNECT_UNAVAILABLE";
    case 4: return "MQTT_CONNECT_BAD_CREDENTIALS";
    case 5: return "MQTT_CONNECT_UNAUTHORIZED";
    default: return "MQTT_UNKNOWN_ERROR";
  }
}

/**
 * @brief Sync time with NTP servers
 */
boolean SensorSentinel_mqtt_sync_time(long timezone, int daylightOffset,
                      const char* ntpServer1, const char* ntpServer2, 
                      const char* ntpServer3) {
  if (!SensorSentinel_wifi_connected()) {
    Serial.println("ERROR: Cannot sync time - WiFi not connected");
    return false;
  }
  
  Serial.printf("Configuring time sync with timezone offset: %ld seconds\n", timezone);
  configTime(timezone, daylightOffset, ntpServer1, ntpServer2, ntpServer3);
  
  Serial.println("Waiting for NTP time sync...");
  
  // Wait up to 5 seconds for time to be set
  unsigned long startAttempt = millis();
  while (time(nullptr) < TIME_SYNC_EPOCH && millis() - startAttempt < 5000) {
    delay(100);
  }
  
  if (time(nullptr) > TIME_SYNC_EPOCH) {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    char timeString[25];
    strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);
    
    Serial.print("NTP time synchronized: ");
    Serial.println(timeString);
    return true;
  } else {
    Serial.println("ERROR: Failed to sync time via NTP after 5 seconds");
    return false;
  }
}

/**
 * @brief Sync time with default parameters
 */
boolean SensorSentinel_mqtt_sync_time_default() {  // Renamed to avoid ambiguity
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
 * @brief Get a unique client ID for MQTT based on ESP32's MAC address
 */
String SensorSentinel_mqtt_get_client_id() {
  // Create a unique client ID based on MAC address
  String macAddress = SensorSentinel_wifi_mac();
  String clientId;
  
  if (macAddress.length() < 9) {
    Serial.println("ERROR: Failed to get valid MAC address for client ID");
    clientId = "HeltecGW-Fallback";
  } else {
    clientId = "HeltecGW-";
    clientId += macAddress.substring(9);
    clientId.replace(":", "");
  }
  
  Serial.printf("MQTT Client ID: %s\n", clientId.c_str());
  return clientId;
}

/**
 * @brief Initialize the MQTT client with server settings
 */
boolean SensorSentinel_mqtt_init() {
  Serial.println("Initializing MQTT client...");
  
  // Validate MQTT server settings
  if (strlen(mqtt_server) == 0) {
    Serial.println("ERROR: MQTT server address is empty");
    return false;
  }
  
  if (mqtt_port <= 0 || mqtt_port > 65535) {
    Serial.printf("ERROR: Invalid MQTT port: %d\n", mqtt_port);
    return false;
  }
  
  // Configure the MQTT client with server and port
  mqttClient.setServer(mqtt_server, mqtt_port);
  
  // Test client ID generation
  mqttClientId = SensorSentinel_mqtt_get_client_id();
  
  // Log the MQTT configuration
  Serial.printf("MQTT Server: %s:%d\n", mqtt_server, mqtt_port);
  Serial.printf("MQTT STATUS Topic: %s\n", mqtt_status_topic);
  Serial.printf("MQTT Buffer Size: %u bytes\n", mqttClient.getBufferSize());
  
  return true;
}

/**
 * @brief Connect to the MQTT broker
 */
boolean SensorSentinel_mqtt_connect() {
  // Check WiFi connection first
  if (!SensorSentinel_wifi_connected()) {
    Serial.println("ERROR: Cannot connect to MQTT - WiFi not connected");
    return false;
  }
  
  // Check if already connected
  if (mqttClient.connected()) {
    return true;
  }
  
  // Don't try to connect too frequently
  unsigned long now = millis();
  if (now - lastMqttConnectionAttempt < mqttConnectionInterval && lastMqttConnectionAttempt > 0) {
    // Just report current state without attempting connection
    if (mqttClient.state() != MQTT_DISCONNECTED) {
      Serial.printf("MQTT connection in progress, state: %d\n", mqttClient.state());
    }
    return false;
  }
  
  lastMqttConnectionAttempt = now;
  
  // Get client ID
  String clientId = SensorSentinel_mqtt_get_client_id();
  
  // Show connection attempt
  Serial.printf("Connecting to MQTT broker %s as %s...", mqtt_server, clientId.c_str());
  
  // Set a shorter connection timeout
  mqttClient.setSocketTimeout(10); // 10 seconds socket timeout instead of default 15
  
  // Connect with or without credentials
  boolean connectResult;
  if (strlen(mqtt_user) > 0) {
    // Use credentials
    connectResult = mqttClient.connect(clientId.c_str(), mqtt_user, mqtt_password);
    if (!connectResult) {
      Serial.printf("Failed with credentials, retrying without...");
      delay(500); // Brief delay before retry
      connectResult = mqttClient.connect(clientId.c_str()); // Try without credentials
    }
  } else {
    // No credentials
    connectResult = mqttClient.connect(clientId.c_str());
  }
  
  if (connectResult) {
    Serial.println("MQTT Connected!");
    Serial.printf("Server: %s\n", mqtt_server);
    Serial.printf("Client: %s\n", clientId.c_str());
    return true;
  } else {
    int mqttState = mqttClient.state();
    Serial.printf("failed, rc=%d ", mqttState);
    
    // Provide more detailed error message
    switch (mqttState) {
      case -4: Serial.println("(MQTT_CONNECTION_TIMEOUT)"); break;
      case -3: Serial.println("(MQTT_CONNECTION_LOST)"); break;
      case -2: Serial.println("(MQTT_CONNECT_FAILED)"); break;
      case -1: Serial.println("(MQTT_DISCONNECTED)"); break;
      case 1: Serial.println("(MQTT_CONNECT_BAD_PROTOCOL)"); break;
      case 2: Serial.println("(MQTT_CONNECT_BAD_CLIENT_ID)"); break;
      case 3: Serial.println("(MQTT_CONNECT_UNAVAILABLE)"); break;
      case 4: Serial.println("(MQTT_CONNECT_BAD_CREDENTIALS)"); break;
      case 5: Serial.println("(MQTT_CONNECT_UNAUTHORIZED)"); break;
      default: Serial.printf("(Unknown error %d)\n", mqttState);
    }
    
    // Update display with error
    heltec_battery_percent();
    both.println("MQTT Connection Fail");
    both.printf("Error: %d\n", mqttState);
    both.printf("Retry in %d sec\n", mqttConnectionInterval/1000);
    heltec_display_update();
    delay(2000);
    return false;
  }
}

/**
 * @brief Setup MQTT with optional time synchronization
 */
boolean SensorSentinel_mqtt_setup(boolean syncTimeOnConnect) {
  
  // Initialize MQTT client
  if (!SensorSentinel_mqtt_init()) {
    Serial.println("ERROR: MQTT initialization failed");
    return false;
  }
  
  // Only proceed if WiFi is connected
  if (!SensorSentinel_wifi_connected()) {
    Serial.println("ERROR: WiFi not connected - MQTT setup deferred");
    return false;
  }
  
  // Sync time if requested
  if (syncTimeOnConnect) {
    Serial.println("Syncing time...");
    
    // Use the timezone defined in platformio.ini
    #ifdef TIMEZONE_OFFSET
    long timezone = TIMEZONE_OFFSET;
    Serial.printf("Using timezone offset from config: %ld seconds\n", timezone);
    if (!SensorSentinel_mqtt_sync_time(timezone, 0, "pool.ntp.org", "time.nist.gov", "time.google.com")) {
      Serial.println("WARNING: Time sync failed, continuing with unsynchronized time");
    }
    #else
    // Fallback if not defined
    Serial.println("WARNING: TIMEZONE_OFFSET not defined, using default UTC+0");
    if (!SensorSentinel_mqtt_sync_time(0, 0, "pool.ntp.org", "time.nist.gov", "time.google.com")) {  // Default to UTC
      Serial.println("WARNING: Time sync failed, continuing with unsynchronized time");
    }
    #endif
  }
  
  // Connect to MQTT broker
  boolean connected = SensorSentinel_mqtt_connect();
  if (connected) {
    Serial.println("MQTT setup completed successfully");
  } else {
    Serial.println("MQTT setup completed but broker connection failed - will retry later");
  }
  
  return connected;
}

/**
 * @brief Maintain MQTT connection - call this regularly in loop()
 */
boolean SensorSentinel_mqtt_maintain() {
  // First check WiFi connectivity
  if (!SensorSentinel_wifi_maintain()) {
    // WiFi is not connected, so MQTT can't work
    return false;
  }
  
  // Check MQTT connection
  if (!mqttClient.connected()) {
    // Try to reconnect at defined interval
    unsigned long now = millis();
    if (now - lastMqttConnectionAttempt > mqttConnectionInterval) {
      Serial.println("MQTT disconnected, attempting reconnection...");
      if (SensorSentinel_mqtt_connect()) {
        Serial.println("MQTT reconnected successfully");
      } else {
        Serial.printf("MQTT reconnection fail, will retry in %u seconds (state=%d)\n", 
                     mqttConnectionInterval/1000, mqttClient.state());
      }
    }
    return false;  // Not connected right now
  } else {
    // Client connected - process MQTT messages
    mqttClient.loop();
    return true;
  }
}

/**
 * @brief Publish a message to an MQTT topic
 */
boolean SensorSentinel_mqtt_publish(const char* topic, const char* payload, boolean retained) {
  // Validate MQTT connection
  if (!mqttClient.connected()) {
    Serial.printf("ERROR: Cannot publish to %s - MQTT not connected\n", topic);
    return false;
  }
  
  // Validate topic
  if (topic == NULL || strlen(topic) == 0) {
    Serial.println("ERROR: Cannot publish - empty topic");
    return false;
  }
  
  // Validate payload
  if (payload == NULL) {
    Serial.printf("ERROR: Cannot publish to %s - NULL payload\n", topic);
    return false;
  }
  
  // Calculate payload size for logging
  size_t payloadLength = strlen(payload);
  
  // Ensure payload isn't too large for MQTT client
  if (payloadLength > mqttClient.getBufferSize()) {
    Serial.printf("ERROR: Payload too large (%u bytes, max %u) for topic %s\n", 
                 payloadLength, mqttClient.getBufferSize(), topic);
    return false;
  }
  
  // Log publication attempt
  if (payloadLength > 100) {
    // For long payloads, just show the length and beginning
    Serial.printf("Publishing to %s: [%u bytes] %.50s...\n", 
                 topic, payloadLength, payload);
  } else {
    // For shorter payloads, show the entire content
    Serial.printf("Publishing to %s: %s\n", topic, payload);
  }
  
  // Attempt to publish
  boolean result = mqttClient.publish(topic, payload, retained);
  
  // Handle publish result
  if (!result) {
    Serial.printf("ERROR: Publish failed to topic %s (state=%d)\n", 
                 topic, mqttClient.state());
  } else {
    // Update last publish time for monitoring
    lastPublishTime = millis();
    publishCount++;
  }
  
  return result;
}

/**
 * @brief Get the MQTT client object
 */
PubSubClient& SensorSentinel_mqtt_get_client() {
  return mqttClient;
}