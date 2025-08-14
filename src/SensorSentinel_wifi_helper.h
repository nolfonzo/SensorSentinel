/**
 * @file SensorSentinel_wifi_helper.h
 * @brief Helper functions for WiFi connectivity
 */

#ifndef SensorSentinel_WIFI_HELPER_H
#define SensorSentinel_WIFI_HELPER_H

#include <Arduino.h>
#include <WiFi.h>

// Forward declarations
class PrintSplitter;
extern PrintSplitter both;
extern WiFiClient wifiClient;

// Essential WiFi functions
bool SensorSentinel_wifi_begin(uint8_t maxAttempts = 20);
bool SensorSentinel_wifi_begin_manager();
bool SensorSentinel_wifi_maintain();
bool SensorSentinel_wifi_connected();

#endif // SensorSentinel_WIFI_HELPER_H