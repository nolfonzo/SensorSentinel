/**
 * @file SensorSentinel_wifi_helper.cpp
 * @brief Implementation of WiFi helper functions
 */

#include "SensorSentinel_wifi_helper.h"
#include "heltec_unofficial_revised.h" // For access to the display functions

// Global WiFi variables
WiFiClient wifiClient;
unsigned long _last_wifi_attempt = 0;
const unsigned long _wifi_retry_interval = 30000; // 30 seconds between retries

// Critical constants
#define WIFI_CONNECT_TIMEOUT 15000  // 15 seconds timeout

/**
 * @brief Connect to WiFi network
 * 
 * @param maxAttempts Maximum number of connection attempts
 * @return true if connected, false otherwise
 */
bool SensorSentinel_wifi_begin(uint8_t maxAttempts) {
    if (WiFi.status() == WL_CONNECTED) {
        return true;
    }
    
    WiFi.mode(WIFI_STA);
    
    // Set hostname
    #ifdef WIFI_HOSTNAME
    WiFi.setHostname(WIFI_HOSTNAME);
    #else
    String hostname = "heltec-" + WiFi.macAddress().substring(9, 17);
    hostname.replace(":", "");
    WiFi.setHostname(hostname.c_str());
    #endif
    
    Serial.printf("Connecting to WiFi: %s\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    // Wait for connection with simple delay
    for (uint8_t i = 0; i < maxAttempts && WiFi.status() != WL_CONNECTED; i++) {
        delay(1000);
        Serial.print(".");
    }
    Serial.println();
    
    bool connected = (WiFi.status() == WL_CONNECTED);
    if (connected) {
        Serial.printf("WiFi connected! IP: %s, RSSI: %d dBm\n", 
                     WiFi.localIP().toString().c_str(), WiFi.RSSI());
    } else {
        Serial.println("WiFi connection failed");
    }
    
    _last_wifi_attempt = millis();
    return connected;
}

/**
 * @brief Maintain WiFi connection
 * Automatically attempts to reconnect if disconnected
 * 
 * @return Current connection status
 */
bool SensorSentinel_wifi_maintain() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }
  
  unsigned long now = millis();
  if (now - _last_wifi_attempt > _wifi_retry_interval) {
    _last_wifi_attempt = now;
    Serial.println("WiFi disconnected, attempting to reconnect");
    
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    delay(500);
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("WiFi reconnected successfully");
    }
  }
  
  return WiFi.status() == WL_CONNECTED;
}

/**
 * @brief Check if WiFi is connected
 * 
 * @return Connection status
 */
bool SensorSentinel_wifi_connected() {
  return WiFi.status() == WL_CONNECTED;
}

/**
 * @brief Get WiFi signal strength
 * 
 * @return RSSI value in dBm, or 0 if not connected
 */
int SensorSentinel_wifi_rssi() {
    return WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
}

/**
 * @brief Disconnect from WiFi network
 */
void SensorSentinel_wifi_disconnect() {
  WiFi.disconnect(true);
}

/**
 * @brief Get the current IP address
 * 
 * @return IP address as String, or "0.0.0.0" if not connected
 */
String SensorSentinel_wifi_ip() {
    return WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "0.0.0.0";
}

/**
 * @brief Get the MAC address
 * 
 * @return MAC address as String
 */
String SensorSentinel_wifi_mac() {
    return WiFi.macAddress();
}

/**
 * @brief Set the hostname for the device
 * 
 * @param hostname The hostname to set
 */
void SensorSentinel_wifi_hostname(const char* hostname) {
  WiFi.setHostname(hostname);
}

/**
 * @brief Get WiFi status as a string
 * 
 * @return Human-readable WiFi status
 */
String SensorSentinel_wifi_status_string() {
    static const char* status_strings[] = {
        "Idle", "No SSID", "Scan Completed", "Connected", 
        "Failed", "Lost", "Disconnected"
    };
    
    int status = WiFi.status();
    if (status >= 0 && status < 7) {
        return String(status_strings[status]);
    }
    return "Unknown";
}

/**
 * @brief Scan for nearby WiFi networks
 * @param showOnDisplay Whether to show results on the display
 * @return Number of networks found
 */
int SensorSentinel_wifi_scan() {
    Serial.println("Scanning for WiFi networks...");
    
    int networksFound = WiFi.scanNetworks();
    
    Serial.printf("Found %d networks:\n", networksFound);
    for (int i = 0; i < networksFound; i++) {
        Serial.printf("%d: %s (%d dBm) %s\n", i + 1, 
                    WiFi.SSID(i).c_str(), 
                    WiFi.RSSI(i),
                    WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "Open" : "Encrypted");
    }
    
    return networksFound;
}

/**
 * @brief Get scan result details for a specific network
 * 
 * @param index Network index (0-based)
 * @return Formatted string with network details or empty string if invalid
 */
String SensorSentinel_wifi_scan_result(int index) {
  if (index < 0 || index >= WiFi.scanComplete()) {
    return "";
  }
  
  char result[64];
  snprintf(result, sizeof(result), "%s,%d,%d", 
          WiFi.SSID(index).c_str(), 
          WiFi.RSSI(index),
          WiFi.encryptionType(index));
  
  return String(result);
}