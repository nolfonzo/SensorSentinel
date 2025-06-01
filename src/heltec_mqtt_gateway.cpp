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

boolean heltec_mqtt_sync_time(long timezone, int daylightOffset,
                      const char* ntpServer1, const char* ntpServer2, 
                      const char* ntpServer3) {
  if (heltec_wifi_connected()) {
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
      Serial.println("Failed to sync time via NTP");
      return false;
    }
  }
  
  Serial.println("Cannot sync time - WiFi not connected");
  return false;
}

void heltec_mqtt_init() {
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  
  // Create a unique client ID based on MAC address
  mqttClientId = "HeltecGW-";
  mqttClientId += heltec_wifi_mac().substring(9);
  mqttClientId.replace(":", "");
  
  Serial.println("MQTT initialized with server: " + String(MQTT_SERVER));
  Serial.println("MQTT client ID: " + mqttClientId);
}

boolean heltec_mqtt_connect() {
  heltec_clear_display(1, 0);
  both.println("Connecting to MQTT");
  both.println(MQTT_SERVER);
  
  // Connect to MQTT broker
  boolean result = mqttClient.connect(mqttClientId.c_str());
  
  if (result) {
    both.println("MQTT Connected!");
    
    // Publish a connection message
    JsonDocument doc;
    doc["status"] = "online";
    doc["gateway_id"] = mqttClientId;
    doc["board"] = heltec_get_board_name();
    
    // Check size first to avoid buffer overflow
    size_t jsonSize = measureJson(doc);
    if (jsonSize > 256) {
      Serial.printf("Warning: Connection message too large (%d bytes)\n", jsonSize);
      return result; // Still return connection result even if publishing fails
    }
    
    char buffer[256];
    serializeJson(doc, buffer);
    mqttClient.publish(MQTT_STATUS, buffer, true); // retained message
  } else {
    both.printf("Failed, rc=%d\n", mqttClient.state());
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
      heltec_mqtt_sync_time();
    }
    
    // Connect to MQTT
    return heltec_mqtt_connect();
  } else {
    Serial.println("WiFi not connected - MQTT setup deferred");
    return false;
  }
}

boolean heltec_mqtt_maintain() {
  // Only attempt MQTT connection if WiFi is connected
  if (heltec_wifi_connected()) {
    if (!mqttClient.connected()) {
      // Try to reconnect every 5 seconds
      unsigned long now = millis();
      if (now - lastMqttReconnectAttempt > 5000) {
        lastMqttReconnectAttempt = now;
        if (heltec_mqtt_connect()) {
          lastMqttReconnectAttempt = 0;
          Serial.println("MQTT reconnected successfully");
        }
      }
    } else {
      // Client connected - process MQTT messages
      mqttClient.loop();
    }
    
    return mqttClient.connected();
  }
  
  return false;
}

void heltec_mqtt_add_timestamp(JsonDocument& doc, bool useFormattedTime) {
  // Always include millis timestamp (milliseconds since boot)
  doc["timestamp_ms"] = millis();
  
  // Check if we have a synchronized time source
  if (time(nullptr) > TIME_SYNC_EPOCH) { // Using named constant
    // Add Unix timestamp (seconds since epoch)
    time_t now = time(nullptr);
    doc["timestamp"] = (uint32_t)now;
    
    // Add formatted time if requested
    if (useFormattedTime) {
      char timeString[25];
      struct tm timeinfo;
      localtime_r(&now, &timeinfo);
      strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);
      doc["time"] = timeString;
    }
  }
}

boolean heltec_mqtt_publish(const char* topic, const char* payload, boolean retained) {
  if (mqttClient.connected()) {
    return mqttClient.publish(topic, payload, retained);
  }
  return false;
}

boolean heltec_mqtt_publish_json(const char* topic, JsonDocument& doc, boolean retained, boolean useFormattedTime) {
  if (mqttClient.connected()) {
    // Add timestamp information
    heltec_mqtt_add_timestamp(doc, useFormattedTime);
    
    // Check size first to avoid buffer overflow
    size_t jsonSize = measureJson(doc);
    if (jsonSize > 512) {
      Serial.printf("Warning: JSON too large (%d bytes)\n", jsonSize);
      return false;
    }
    
    char buffer[512];
    serializeJson(doc, buffer);
    return mqttClient.publish(topic, buffer, retained);
  }
  return false;
}

boolean heltec_mqtt_publish_status(uint32_t packetCounter, JsonDocument* extraInfo, boolean useFormattedTime) {
  if (mqttClient.connected()) {
    JsonDocument statusDoc;
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
    }
    
    // Add any extra fields if provided
    if (extraInfo != nullptr) {
      // Convert to string and parse back to avoid range-based for loop issues
      String extraJson;
      serializeJson(*extraInfo, extraJson);
      JsonDocument tempDoc;
      deserializeJson(tempDoc, extraJson);
      
      JsonObject obj = tempDoc.as<JsonObject>();
      for (JsonPair p : obj) {
        statusDoc[p.key().c_str()] = p.value();
      }
    }
    
    // Publish - note we don't use heltec_mqtt_publish_json here to avoid
    // double-adding timestamps since we've already handled them
    
    // Check size first to avoid buffer overflow
    size_t jsonSize = measureJson(statusDoc);
    if (jsonSize > 512) {
      Serial.printf("Warning: Status JSON too large (%d bytes)\n", jsonSize);
      return false;
    }
    
    char buffer[512];
    serializeJson(statusDoc, buffer);
    return mqttClient.publish(MQTT_STATUS, buffer, true); // retained message
  }
  return false;
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
  }
  
  both.printf("Packets: %d\n", packetCounter);
  both.printf("WiFi: %s\n", heltec_wifi_connected() ? "Connected" : "Disconnected");
  both.printf("MQTT: %s\n", mqttClient.connected() ? "Connected" : "Disconnected");
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