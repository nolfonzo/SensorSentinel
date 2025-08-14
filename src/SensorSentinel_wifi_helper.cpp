/**
 * @file SensorSentinel_wifi_helper.cpp
 * @brief Implementation of WiFi helper functions
 */

#include "SensorSentinel_wifi_helper.h"
#include "heltec_unofficial_revised.h"
#include <WiFiManager.h>

WiFiClient wifiClient;

/**
 * @brief Simple 2-step WiFi: WiFiManager → Hardcoded fallback
 */
bool SensorSentinel_wifi_begin(uint8_t maxAttempts) {
    if (WiFi.status() == WL_CONNECTED) {
        return true;
    }
    
    // Step 1: WiFiManager (handles stored credentials automatically)
    Serial.println("Starting WiFiManager (checks stored credentials first)...");
    WiFiManager wm;
    String apName = "SensorSentinel-" + String(ESP.getEfuseMac() >> 16, HEX);
    wm.setConfigPortalTimeout(120); // 2 minutes
    
    if (wm.autoConnect(apName.c_str())) {
        Serial.printf("WiFi connected: %s\n", WiFi.localIP().toString().c_str());
        return true;
    }
    
    // Step 2: Hardcoded fallback
    Serial.println("WiFiManager failed, trying hardcoded...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    for (int i = 0; i < maxAttempts && WiFi.status() != WL_CONNECTED; i++) {
        delay(500);
        Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\nConnected via hardcoded: %s\n", WiFi.localIP().toString().c_str());
        return true;
    }
    
    Serial.println("\nAll WiFi methods failed");
    return false;
}

bool SensorSentinel_wifi_maintain() {
    if (WiFi.status() == WL_CONNECTED) {
        return true;
    }
    
    static unsigned long lastAttempt = 0;
    if (millis() - lastAttempt > 30000) {
        lastAttempt = millis();
        Serial.println("WiFi disconnected, reconnecting...");
        WiFi.reconnect();
    }
    
    return WiFi.status() == WL_CONNECTED;
}

bool SensorSentinel_wifi_connected() {
    return WiFi.status() == WL_CONNECTED;
}