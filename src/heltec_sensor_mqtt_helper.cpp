/**
 * @file heltec_sensor_mqtt_helper.cpp
 * @brief Implementation of sensor packet to MQTT gateway functions
 */

#include "heltec_unofficial.h"
#include "heltec_mqtt_gateway.h"
#include "heltec_wifi_helper.h"
#include "heltec_sensor_packet.h"
#include "heltec_sensor_mqtt_helper.h"

// Always use the same topic for all packet types
const char* topic = MQTT_TOPIC;

// Stats for processed packets
static uint32_t totalPacketsReceived = 0;
static uint32_t lastNodeId = 0;
static uint32_t lastCounter = 0;
static uint8_t lastBattery = 0;
static uint8_t lastMessageType = 0;
static float lastLatitude = 0;
static float lastLongitude = 0;
static bool lastHasGnss = false;
static int16_t lastRssi = 0;
static uint8_t lastSnr = 0;

/**
 * @brief Publish a sensor packet to MQTT
 * 
 * @param packet Pointer to the sensor packet
 * @param rssi Optional RSSI value (overrides packet value if non-zero)
 * @param snr Optional SNR value (overrides packet value if non-zero)
 * @return boolean True if published successfully
 */
boolean heltec_sensor_mqtt_publish(const heltec_sensor_packet_t* packet, int16_t rssi, uint8_t snr) {
    if (packet == nullptr) {
        Serial.println("Error: Null packet pointer in heltec_sensor_mqtt_publish");
        return false;
    }
    
    // Store stats for later retrieval
    lastNodeId = packet->nodeId;
    lastCounter = packet->messageCounter;
    lastBattery = packet->batteryPercent;
    lastMessageType = packet->messageType;
    lastRssi = (rssi != 0) ? rssi : packet->rssi;
    lastSnr = (snr != 0) ? snr : packet->snr;
    
    // Save location data if present
    if (packet->messageType == HELTEC_MSG_GNSS) {
        lastHasGnss = true;
        lastLatitude = packet->latitude;
        lastLongitude = packet->longitude;
    } else {
        lastHasGnss = false;
    }
    
    // Increment total packet count
    totalPacketsReceived++;
    
    // Create a JSON document for the packet
    JsonDocument doc;
    
    // Use the existing function to populate the document
    if (!heltec_packet_to_json_doc(packet, doc)) {
        Serial.println("Error: Failed to convert packet to JSON");
        return false;
    }
    
    // Override RSSI and SNR if provided
//    if (rssi != 0) {
  //      doc["rssi"] = rssi;
  //  }
  //  if (snr != 0) {
  //      doc["snr"] = snr;
  //  }
    
    // Add gateway information
    doc["gateway_id"] = heltec_mqtt_get_client_id();
    doc["gateway_mac"] = heltec_wifi_mac();
    
    // Print debug information
    Serial.printf("Publishing sensor packet to MQTT topic: %s\n",MQTT_TOPIC);
    Serial.printf("  Node ID: 0x%08X\n", packet->nodeId);
    Serial.printf("  Counter: %u\n", packet->messageCounter);
    Serial.printf("  Type: %s\n", (packet->messageType == HELTEC_MSG_GNSS) ? "GNSS" : "Sensor");
    Serial.printf("  Battery: %u%%\n", packet->batteryPercent);
    Serial.printf("  RSSI: %d dBm\n", lastRssi);
    
    // Publish to MQTT using the existing gateway function
    return heltec_mqtt_publish_json(topic, doc);
}

/**
 * @brief Process and publish a raw binary packet
 * 
 * @param data Raw binary data
 * @param length Length of the data
 * @param rssi RSSI value
 * @param snr SNR value
 * @return boolean True if successfully parsed and published
 */
boolean heltec_sensor_mqtt_publish_raw(const uint8_t* data, size_t length, int16_t rssi, uint8_t snr) {
    if (data == nullptr || length == 0) {
        Serial.println("Error: Invalid data in heltec_sensor_mqtt_publish_raw");
        return false;
    }
    
    // Parse the raw data into a sensor packet
    heltec_sensor_packet_t packet;
    if (!heltec_parse_packet((uint8_t*)data, length, &packet)) {
        Serial.println("Error: Failed to parse sensor packet");
        return false;
    }
    
    // Publish the parsed packet
    return heltec_sensor_mqtt_publish(&packet, rssi, snr);
}

/**
 * @brief Process a packet from LoRa callback
 * 
 * @param data String containing the raw data
 * @param rssi RSSI value
 * @param snr SNR value
 * @return boolean True if successfully processed and published
 */
boolean heltec_sensor_mqtt_process(String &data, float rssi, float snr) {
    // Convert String to byte array
    uint8_t rawData[256]; // Maximum possible packet size
    size_t dataLen = data.length();
    
    if (dataLen > sizeof(rawData)) {
        Serial.printf("Error: Data too large (%d bytes)\n", dataLen);
        return false;
    }
    
    // Copy String data to byte array
    memcpy(rawData, data.c_str(), dataLen);
    
    // Convert floating point RSSI and SNR to their integer representations
    int16_t rssiInt = (int16_t)rssi;
    uint8_t snrInt = (uint8_t)snr;
    
    // Process and publish the packet
    return heltec_sensor_mqtt_publish_raw(rawData, dataLen, rssiInt, snrInt);
}

/**
 * @brief Add sensor packet information to status document
 * 
 * @param doc JSON document to add information to
 */
void heltec_sensor_mqtt_add_status(JsonDocument& doc) {
    // Only add if we've received a packet
    if (lastNodeId != 0) {
        JsonObject lastNode = doc["last_sensor"].to<JsonObject>();
        lastNode["id"] = lastNodeId;
        
        char nodeIdHex[11];
        sprintf(nodeIdHex, "0x%08X", lastNodeId);
        lastNode["id_hex"] = nodeIdHex;
        
        lastNode["counter"] = lastCounter;
        lastNode["battery"] = lastBattery;
        lastNode["rssi"] = lastRssi;
        lastNode["snr"] = lastSnr;
        
        switch (lastMessageType) {
            case HELTEC_MSG_BASIC:
                lastNode["type"] = "sensor";
                break;
            case HELTEC_MSG_GNSS:
                lastNode["type"] = "gnss";
                if (lastHasGnss) {
                    JsonObject location = lastNode["location"].to<JsonObject>();
                    location["lat"] = lastLatitude;
                    location["lon"] = lastLongitude;
                }
                break;
            default:
                lastNode["type"] = "unknown";
                break;
        }
    }
    
    // Add packet statistics
    //doc["sensor_packets"] = totalPacketsReceived;
}

/**
 * @brief Get statistics about processed sensor packets
 * 
 * @param packetCount Pointer to variable to receive total packet count
 * @param lastNodeId Pointer to variable to receive last node ID
 * @param lastCounter Pointer to variable to receive last packet counter
 */
void heltec_sensor_mqtt_get_stats(uint32_t* packetCount, uint32_t* nodeId, uint32_t* counter) {
    if (packetCount) {
        *packetCount = totalPacketsReceived;
    }
    if (nodeId) {
        *nodeId = lastNodeId;
    }
    if (counter) {
        *counter = lastCounter;
    }
}