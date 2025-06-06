/**
 * @file SensorSentinel_wifi_helper.h
 * @brief Helper functions for WiFi connectivity
 */

#ifndef SensorSentinel_WIFI_HELPER_H
#define SensorSentinel_WIFI_HELPER_H

#include <Arduino.h>
#include <WiFi.h>

// Forward declaration for PrintSplitter from heltec_unofficial_revised.h
class PrintSplitter;
extern PrintSplitter both;

// Declare external WiFiClient
extern WiFiClient wifiClient;

// Function to clear display - forward declaration
void heltec_clear_display(uint8_t textSize, uint8_t rotation);

// WiFi connection tracking
extern bool _wifi_connected;
extern unsigned long _last_wifi_attempt;
extern const unsigned long _wifi_retry_interval;

// Public function declarations - these are the only ones that should be in the header
bool SensorSentinel_wifi_begin(uint8_t maxAttempts = 20);
bool SensorSentinel_wifi_maintain();
bool SensorSentinel_wifi_connected();
int SensorSentinel_wifi_rssi();
void SensorSentinel_wifi_disconnect();
String SensorSentinel_wifi_ip();
String SensorSentinel_wifi_mac();
void SensorSentinel_wifi_hostname(const char* hostname);
String SensorSentinel_wifi_status_string();
int SensorSentinel_wifi_scan(bool showOnDisplay = true);
String SensorSentinel_wifi_scan_result(int index);

#endif // SensorSentinel_WIFI_HELPER_H