/**
 * @file heltec_sensor_mqtt_helper.h
 * @brief Connects sensor packets to MQTT gateway
 * 
 * This module provides functions to convert sensor packets to MQTT messages
 * using the generic MQTT gateway functionality.
 */

#ifndef HELTEC_SENSOR_MQTT_HELPER_H
#define HELTEC_SENSOR_MQTT_HELPER_H

#include <Arduino.h>
#include "Heltec_Sensor_Packet.h"

/**
 * @brief Publish a sensor packet to MQTT
 * 
 * @param packet Pointer to the sensor packet
 * @param rssi Optional RSSI value (overrides packet value if non-zero)
 * @param snr Optional SNR value (overrides packet value if non-zero)
 * @return boolean True if published successfully
 */
boolean heltec_sensor_mqtt_publish(const heltec_sensor_packet_t* packet, int16_t rssi = 0, uint8_t snr = 0);

/**
 * @brief Process and publish a raw binary packet
 * 
 * @param data Raw binary data
 * @param length Length of the data
 * @param rssi RSSI value
 * @param snr SNR value
 * @return boolean True if successfully parsed and published
 */
boolean heltec_sensor_mqtt_publish_raw(const uint8_t* data, size_t length, int16_t rssi, uint8_t snr);

/**
 * @brief Process a packet from LoRa callback
 * 
 * @param data String containing the raw data
 * @param rssi RSSI value
 * @param snr SNR value
 * @return boolean True if successfully processed and published
 */
boolean heltec_sensor_mqtt_process(String &data, float rssi, float snr);

/**
 * @brief Add sensor packet information to status document
 * 
 * @param doc JSON document to add information to
 */
void heltec_sensor_mqtt_add_status(JsonDocument& doc);

/**
 * @brief Get statistics about processed sensor packets
 * 
 * @param packetCount Pointer to variable to receive total packet count
 * @param lastNodeId Pointer to variable to receive last node ID
 * @param lastCounter Pointer to variable to receive last packet counter
 */
void heltec_sensor_mqtt_get_stats(uint32_t* packetCount, uint32_t* lastNodeId, uint32_t* lastCounter);

#endif // HELTEC_SENSOR_MQTT_HELPER_H