/**
 * @file heltec_wifi_helper.cpp
 * @brief Implementation of WiFi helper functions for Heltec boards
 */

#include "heltec_wifi_helper.h"
#include "heltec_unofficial.h" // For access to the display functions

// Global WiFi variables
WiFiClient wifiClient;
bool _wifi_connected = false;
unsigned long _last_wifi_attempt = 0;
const unsigned long _wifi_retry_interval = 30000; // 30 seconds between retries
unsigned long _last_scan_time = 0;

// Critical constants
#define WIFI_CONNECT_TIMEOUT 15000  // 15 seconds timeout
#define SCAN_CACHE_TIMEOUT 60000    // 1 minute scan cache validity

// Forward declaration of internal helper function
static void displayScanResults(int networksFound);

/**
 * @brief Connect to WiFi network
 * 
 * @param maxAttempts Maximum number of connection attempts
 * @return true if connected, false otherwise
 */
bool heltec_wifi_begin(uint8_t maxAttempts) {
  if (WiFi.status() == WL_CONNECTED) {
    _wifi_connected = true;
    return true;
  }
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  // Set hostname if defined, otherwise use MAC-based name
  #ifdef WIFI_HOSTNAME
    WiFi.setHostname(WIFI_HOSTNAME);
  #else
    String hostname = "heltec-";
    hostname += WiFi.macAddress().substring(9);
    hostname.replace(":", "");
    WiFi.setHostname(hostname.c_str());
  #endif
  
  heltec_clear_display(1, 1);
  both.print("Connecting to WiFi SSID: ");
  both.println(WIFI_SSID);

  uint8_t attempts = 0;
  unsigned long startTime = millis();
  
  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    // Add timeout check to prevent blocking too long
    if (millis() - startTime > WIFI_CONNECT_TIMEOUT) {
      Serial.println("WiFi connection timeout");
      both.println("\nConnection timeout");
      _wifi_connected = false;
      return false;
    }
    
    delay(500);
    both.print(".");
    attempts++;
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    _wifi_connected = true;
    _last_wifi_attempt = millis();
  
    both.println("\nWiFi connected!");
    both.print("SSID: ");
    both.println(WIFI_SSID);
    both.print("IP: ");
    both.println(WiFi.localIP().toString());
    both.print("RSSI: ");
    both.print(WiFi.RSSI());
    delay(2000);
    return true;
  } else {
    _wifi_connected = false;
    both.println("\nConnection failed");
    delay(2000);
    return false;
  }
}

/**
 * @brief Maintain WiFi connection
 * Automatically attempts to reconnect if disconnected
 * 
 * @return Current connection status
 */
bool heltec_wifi_maintain() {
  if (WiFi.status() == WL_CONNECTED) {
    _wifi_connected = true;
    return true;
  }
  
  // If we were previously connected, or it's been a while since last attempt
  unsigned long now = millis();
  if (_wifi_connected || (now - _last_wifi_attempt > _wifi_retry_interval)) {
    _last_wifi_attempt = now;
    Serial.println("WiFi disconnected, attempting to reconnect");
    
    // Non-recursive approach to reconnection
    _wifi_connected = false;
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    // Allow WiFi stack to process reconnection
    delay(500);
    
    // Quick check if reconnection was immediate
    if (WiFi.status() == WL_CONNECTED) {
      _wifi_connected = true;
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
bool heltec_wifi_connected() {
  return WiFi.status() == WL_CONNECTED;
}

/**
 * @brief Get WiFi signal strength
 * 
 * @return RSSI value in dBm, or 0 if not connected
 */
int heltec_wifi_rssi() {
  if (WiFi.status() == WL_CONNECTED) {
    return WiFi.RSSI();
  }
  return 0;
}

/**
 * @brief Disconnect from WiFi network
 */
void heltec_wifi_disconnect() {
  WiFi.disconnect(true);
  _wifi_connected = false;
}

/**
 * @brief Get the current IP address
 * 
 * @return IP address as String, or "0.0.0.0" if not connected
 */
String heltec_wifi_ip() {
  if (WiFi.status() == WL_CONNECTED) {
    return WiFi.localIP().toString();
  }
  return "0.0.0.0";
}

/**
 * @brief Get the MAC address
 * 
 * @return MAC address as String
 */
String heltec_wifi_mac() {
  return WiFi.macAddress();
}

/**
 * @brief Set the hostname for the device
 * 
 * @param hostname The hostname to set
 */
void heltec_wifi_hostname(const char* hostname) {
  WiFi.setHostname(hostname);
}

/**
 * @brief Get WiFi status as a string
 * 
 * @return Human-readable WiFi status
 */
String heltec_wifi_status_string() {
  switch (WiFi.status()) {
    case WL_CONNECTED: return "Connected";
    case WL_DISCONNECTED: return "Disconnected";
    case WL_CONNECT_FAILED: return "Connection Failed";
    case WL_NO_SSID_AVAIL: return "SSID Not Available";
    case WL_IDLE_STATUS: return "Idle";
    case WL_SCAN_COMPLETED: return "Scan Completed";
    default: return "Unknown";
  }
}

/**
 * @brief Scan for nearby WiFi networks
 * 
 * @param showOnDisplay Whether to show results on the display
 * @return Number of networks found
 */
int heltec_wifi_scan(bool showOnDisplay) {
  // Check if cached results are still valid
  if (WiFi.scanComplete() >= 0 && millis() - _last_scan_time < SCAN_CACHE_TIMEOUT) {
    Serial.println("Using cached WiFi scan results");
    
    if (showOnDisplay) {
      displayScanResults(WiFi.scanComplete());
    }
    
    return WiFi.scanComplete();
  }
  
  // Perform a new scan
  Serial.println("Scanning for WiFi networks...");
  
  if (showOnDisplay) {
    heltec_clear_display(1, 1);
    both.println("Scanning WiFi...");
  }

  int networksFound = WiFi.scanNetworks();
  _last_scan_time = millis();
  
  Serial.printf("Found %d networks:\n", networksFound);
  
  if (showOnDisplay) {
    displayScanResults(networksFound);
  }

  // Print all networks to serial
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
String heltec_wifi_scan_result(int index) {
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

/**
 * @brief Internal helper function to display scan results
 * 
 * @param networksFound Number of networks found
 */
static void displayScanResults(int networksFound) {
  heltec_clear_display(1, 1);
  both.printf("Found %d networks:\n", networksFound);
  
  // Show first 5 networks on display
  for (int i = 0; i < min(5, networksFound); i++) {
    both.printf("%d: %s (%d dBm)\n", i + 1, 
              WiFi.SSID(i).c_str(), 
              WiFi.RSSI(i));
  }
}