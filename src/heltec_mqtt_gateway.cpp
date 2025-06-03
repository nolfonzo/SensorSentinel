/**
 * @file heltec_mqtt_gateway.cpp
 * @brief Implementation of MQTT helper functions for Heltec boards
 */

#include "heltec_unofficial.h"
#include "heltec_mqtt_gateway.h"
#include "heltec_wifi_helper.h"

// Module variables
static PubSubClient mqttClient(wifiClient);
static String mqttClientId = "";
static unsigned long lastMqttReconnectAttempt = 0;

// Magic constant replaced with a named constant
#define TIME_SYNC_EPOCH 1600000000  // Sept 2020, indicates time is synced

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

boolean heltec_mqtt_sync_time(long timezone, int daylightOffset,
                      const char* ntpServer1, const char* ntpServer2, 
                      const char* ntpServer3) {
  if (!heltec_wifi_connected()) {
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

void heltec_mqtt_init() {
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  
  if (strlen(MQTT_SERVER) == 0) {
    Serial.println("ERROR: MQTT server not configured (empty string)");
    return;
  }
  
  // Create a unique client ID based on MAC address
  String macAddress = heltec_wifi_mac();
  if (macAddress.length() < 9) {
    Serial.println("ERROR: Failed to get valid MAC address for client ID");
    mqttClientId = "HeltecGW-Fallback";
  } else {
    mqttClientId = "HeltecGW-";
    mqttClientId += macAddress.substring(9);
    mqttClientId.replace(":", "");
  }
  
  Serial.printf("MQTT initialized with server: %s:%d\n", MQTT_SERVER, MQTT_PORT);
  Serial.println("MQTT client ID: " + mqttClientId);
}

boolean heltec_mqtt_connect() {
  if (!heltec_wifi_connected()) {
    Serial.println("ERROR: Cannot connect to MQTT - WiFi not connected");
    return false;
  }
  
  heltec_clear_display(1, 0);
  both.println("Connecting to MQTT");
  both.println(MQTT_SERVER);
  
  // Connect to MQTT broker
  Serial.printf("Attempting MQTT connection to %s as %s...\n", MQTT_SERVER, mqttClientId.c_str());
  boolean result = mqttClient.connect(mqttClientId.c_str());
  
  if (result) {
    both.println("MQTT Connected!");
    Serial.println("MQTT connection successful");
    
    // Publish a connection message
    JsonDocument doc;
    doc["status"] = "online";
    doc["gateway_id"] = mqttClientId;
    doc["board"] = heltec_get_board_name();
    
    // Check size first to avoid buffer overflow
    size_t jsonSize = measureJson(doc);
    if (jsonSize > 256) {
      Serial.printf("ERROR: Connection message too large (%d bytes, max 256)\n", jsonSize);
      return result; // Still return connection result even if publishing fails
    }
    
    char buffer[256];
    serializeJson(doc, buffer);
    boolean publishResult = mqttClient.publish(MQTT_STATUS, buffer, true); // retained message
    
    if (!publishResult) {
      Serial.printf("ERROR: Failed to publish initial status message to %s\n", MQTT_STATUS);
    } else {
      Serial.printf("Successfully published initial status to %s\n", MQTT_STATUS);
    }
  } else {
    int state = mqttClient.state();
    Serial.printf("ERROR: MQTT connection failed, state=%d (%s)\n", 
                  state, getMqttStateString(state));
    both.printf("Failed, rc=%d\n", state);
  }
  
  return result;
}

boolean heltec_mqtt_setup(boolean syncTimeOnConnect) {
  // Initialize MQTT
  heltec_mqtt_init();
  
  // Only proceed if WiFi is connected
  if (heltec_wifi_connected()) {
    // Sync time if requested
    if (syncTimeOnConnect) {
      both.println("Syncing time...");
      
      // Use the timezone defined in platformio.ini
      #ifdef TIMEZONE_OFFSET
      long timezone = TIMEZONE_OFFSET;
      Serial.printf("Using timezone offset from config: %ld seconds\n", timezone);
      if (!heltec_mqtt_sync_time(timezone, 0)) {
        Serial.println("WARNING: Time sync failed, continuing with unsynchronized time");
      }
      #else
      // Fallback if not defined
      Serial.println("WARNING: TIMEZONE_OFFSET not defined, using default UTC+10");
      if (!heltec_mqtt_sync_time(36000, 0)) {  // Default to Sydney (UTC+10)
        Serial.println("WARNING: Time sync failed, continuing with unsynchronized time");
      }
      #endif
    }
    
    // Connect to MQTT
    return heltec_mqtt_connect();
  } else {
    Serial.println("ERROR: WiFi not connected - MQTT setup deferred");
    return false;
  }
}

boolean heltec_mqtt_maintain() {
  // Only attempt MQTT connection if WiFi is connected
  if (!heltec_wifi_connected()) {
    // No point trying to maintain MQTT without WiFi
    return false;
  }
  
  if (!mqttClient.connected()) {
    // Try to reconnect every 5 seconds
    unsigned long now = millis();
    if (now - lastMqttReconnectAttempt > 5000) {
      lastMqttReconnectAttempt = now;
      Serial.println("MQTT disconnected, attempting reconnection...");
      if (heltec_mqtt_connect()) {
        lastMqttReconnectAttempt = 0;
        Serial.println("MQTT reconnected successfully");
      } else {
        Serial.printf("MQTT reconnection failed, will retry in 5 seconds (state=%d)\n", 
                     mqttClient.state());
      }
    }
  } else {
    // Client connected - process MQTT messages
    mqttClient.loop();
  }
  
  return mqttClient.connected();
}

void heltec_mqtt_add_timestamp(JsonDocument& doc, bool useFormattedTime) {
  // Always include millis timestamp (milliseconds since boot)
  doc["timestamp_ms"] = millis();
  
  // Check if we have a synchronized time source
  time_t now = time(nullptr);
  if (now > TIME_SYNC_EPOCH) { // Using named constant
    // Add Unix timestamp (seconds since epoch)
    doc["timestamp"] = (uint32_t)now;
    
    // Add formatted time if requested
    if (useFormattedTime) {
      char timeString[25];
      struct tm timeinfo;
      localtime_r(&now, &timeinfo);
      strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);
      doc["time"] = timeString;
    }
  } else {
    Serial.println("WARNING: Using unsynchronized time in document");
  }
}

boolean heltec_mqtt_publish(const char* topic, const char* payload, boolean retained) {
  if (!mqttClient.connected()) {
    Serial.printf("ERROR: Cannot publish to %s - MQTT not connected\n", topic);
    return false;
  }
  
  if (topic == NULL || strlen(topic) == 0) {
    Serial.println("ERROR: Cannot publish - empty topic");
    return false;
  }
  
  if (payload == NULL) {
    Serial.printf("ERROR: Cannot publish to %s - NULL payload\n", topic);
    return false;
  }
  
  Serial.printf("Publishing to %s: %s\n", topic, payload);
  boolean result = mqttClient.publish(MQTT_TOPIC, payload, retained);
  
  if (!result) {
    Serial.printf("ERROR: Publish failed to topic %s (state=%d)\n", 
                 topic, mqttClient.state());
  }
  
  return result;
}

boolean heltec_mqtt_publish_json(const char* topic, JsonDocument& doc, boolean retained, boolean useFormattedTime) {
  if (!mqttClient.connected()) {
    Serial.printf("ERROR: Cannot publish JSON to %s - MQTT not connected\n", topic);
    return false;
  }
  
  if (topic == NULL || strlen(topic) == 0) {
    Serial.println("ERROR: Cannot publish JSON - empty topic");
    return false;
  }
  
  // Add timestamp information
  heltec_mqtt_add_timestamp(doc, useFormattedTime);
  
  // Check size first to avoid buffer overflow
  size_t jsonSize = measureJson(doc);
  Serial.printf("JSON size for topic %s: %d bytes\n", topic, jsonSize);
  
  if (jsonSize > 512) {
    Serial.printf("ERROR: JSON too large for topic %s (%d bytes, max 512)\n", 
                 topic, jsonSize);
    return false;
  }
  
  char buffer[512];
  serializeJson(doc, buffer);
  
  Serial.printf("MQTT_MAX_PACKET_SIZE = %d\n", MQTT_MAX_PACKET_SIZE);
  Serial.printf("Actual serialized JSON length: %d bytes\n", strlen(buffer));
  Serial.printf("MQTT client state before publish: %d (%s)\n", 
                mqttClient.state(), getMqttStateString(mqttClient.state()));


  Serial.println("********about to do a small publish test");
  test_small_json_publish();
  delay(1000);

  boolean result = mqttClient.publish(MQTT_TOPIC, buffer, false);
  
  if (!result) {
    Serial.printf("ERROR: JSON publish failed to topic %s (state=%d)\n", 
                 topic, mqttClient.state());
    // Print the JSON that failed to publish for debugging
    Serial.println("Failed JSON content:");
    serializeJsonPretty(doc, Serial);
    Serial.println();
  } else {
    Serial.printf("Successfully published JSON to topic %s\n", topic);
  }
  
  return result;
}

boolean heltec_mqtt_publish_status(uint32_t packetCounter, JsonDocument* extraInfo, boolean useFormattedTime) {
  if (!mqttClient.connected()) {
    Serial.println("ERROR: Cannot publish status - MQTT not connected");
    return false;
  }
  
  Serial.println("Preparing gateway status message");
  JsonDocument statusDoc;
  
  try {
    statusDoc["status"] = "online";
    statusDoc["uptime_sec"] = millis() / 1000;
    statusDoc["gateway_id"] = mqttClientId;
    statusDoc["board"] = heltec_get_board_name();
    statusDoc["free_heap"] = ESP.getFreeHeap();
    statusDoc["rssi"] = heltec_wifi_rssi();
    statusDoc["battery"] = heltec_battery_percent();
    statusDoc["ip"] = heltec_wifi_ip();
    
    if (packetCounter > 0) {
      statusDoc["received_packets"] = packetCounter;
    }
    
    // Add time information
    time_t now = time(nullptr);
    if (now > TIME_SYNC_EPOCH) { // Using named constant
      // Add Unix timestamp (seconds since epoch)
      statusDoc["timestamp"] = (uint32_t)now;
      
      // Add formatted time string if requested
      if (useFormattedTime) {
        char timeString[25];
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);
        statusDoc["time"] = timeString;
      }
    } else {
      // If time not set, include boot time
      statusDoc["time_sync"] = false;
      Serial.println("WARNING: Time not synchronized for status message");
    }
    
    // Add any extra fields if provided
    if (extraInfo != nullptr) {
      // Convert to string and parse back to avoid range-based for loop issues
      String extraJson;
      serializeJson(*extraInfo, extraJson);
      
      if (extraJson.length() > 0) {
        JsonDocument tempDoc;
        DeserializationError error = deserializeJson(tempDoc, extraJson);
        
        if (error) {
          Serial.printf("ERROR: Failed to parse extra info JSON: %s\n", error.c_str());
        } else {
          JsonObject obj = tempDoc.as<JsonObject>();
          for (JsonPair p : obj) {
            statusDoc[p.key().c_str()] = p.value();
          }
        }
      }
    }
  } catch (const std::exception& e) {
    Serial.printf("ERROR: Exception while preparing status JSON: %s\n", e.what());
    return false;
  }
  
  // Publish - note we don't use heltec_mqtt_publish_json here to avoid
  // double-adding timestamps since we've already handled them
  
  // Check size first to avoid buffer overflow
  size_t jsonSize = measureJson(statusDoc);
  Serial.printf("Status JSON size: %d bytes\n", jsonSize);
  
  if (jsonSize > 512) {
    Serial.printf("ERROR: Status JSON too large (%d bytes, max 512)\n", jsonSize);
    return false;
  }
  
  char buffer[512];
  serializeJson(statusDoc, buffer);
  
  boolean result = mqttClient.publish(MQTT_STATUS, buffer, true); // retained message
  
  if (!result) {
    Serial.printf("ERROR: Status publish failed (state=%d)\n", mqttClient.state());
  } else {
    Serial.println("Successfully published status message");
  }
  
  return result;
}

void heltec_mqtt_display_status(uint32_t packetCounter) {
  heltec_clear_display(1, 0);
  both.println("MQTT Gateway Status");
  
  // Display time if available
  time_t now = time(nullptr);
  if (now > TIME_SYNC_EPOCH) { // Using named constant
    char timeString[20]; // Smaller to fit on display
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M", &timeinfo);
    both.println(timeString);
  } else {
    both.println("Time not synced");
  }
  
  both.printf("Packets: %d\n", packetCounter);
  
  // WiFi status with RSSI if connected
  if (heltec_wifi_connected()) {
    both.printf("WiFi: %ddBm\n", heltec_wifi_rssi());
  } else {
    both.println("WiFi: Disconnected");
  }
  
  // MQTT status with state code if disconnected
  if (mqttClient.connected()) {
    both.println("MQTT: Connected");
  } else {
    both.printf("MQTT: %d\n", mqttClient.state());
  }
  
  both.printf("Uptime: %d min\n", millis() / 60000);
  both.printf("Batt: %d%%\n", heltec_battery_percent());
  
  // Ensure display is updated
  heltec_display_update();
}

// Accessor function for the MQTT client if needed
PubSubClient& heltec_mqtt_get_client() {
  return mqttClient;
}

// Accessor function for the MQTT client ID if needed
String heltec_mqtt_get_client_id() {
  return mqttClientId;
}



