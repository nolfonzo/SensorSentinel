/**
 * @file heltec_mqtt_gateway.h
 * @brief MQTT helper functions for Heltec boards
 */

#ifndef HELTEC_MQTT_GATEWAY_H
#define HELTEC_MQTT_GATEWAY_H

#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>

// Forward declare WiFiClient
class WiFiClient;
extern WiFiClient wifiClient;

// Function declarations
boolean heltec_mqtt_sync_time(long timezone = 0, int daylightOffset = 0,
                             const char* ntpServer1 = "pool.ntp.org",
                             const char* ntpServer2 = "time.nist.gov",
                             const char* ntpServer3 = "time.google.com");
void heltec_mqtt_init();
boolean heltec_mqtt_connect();
boolean heltec_mqtt_setup(boolean syncTimeOnConnect = true);
boolean heltec_mqtt_maintain();
void heltec_mqtt_add_timestamp(JsonDocument& doc, bool useFormattedTime = false);
boolean heltec_mqtt_publish(const char* topic, const char* payload, boolean retained = false);
boolean heltec_mqtt_publish_json(const char* topic, JsonDocument& doc, boolean retained = false, boolean useFormattedTime = false);
boolean heltec_mqtt_publish_status(uint32_t packetCounter = 0, JsonDocument* extraInfo = nullptr, boolean useFormattedTime = true);
void heltec_mqtt_display_status(uint32_t packetCounter = 0);
String heltec_mqtt_get_client_id();

#endif // HELTEC_MQTT_GATEWAY_H