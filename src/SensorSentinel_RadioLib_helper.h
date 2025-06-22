/**
 * @file SensorSentinel_RadioLib_helper.h
 * @brief Helper functions for RadioLib, including subscription
 */

#ifndef SensorSentinel_RadioLib_HELPER_H
#define SensorSentinel_RadioLib_HELPER_H 

#include <heltec_unofficial_revised.h>

/**
 * @brief Callback function type for string packet reception
 * @param data Reference to the received packet data as a String
 * @param rssi Signal strength of the received packet
 * @param snr Signal-to-noise ratio of the received packet
 */
typedef void (*PacketCallback)(String &data, float rssi, float snr);

/**
 * @brief Callback function type for binary packet reception
 * @param data Pointer to the received binary packet data
 * @param length Length of the received binary data in bytes
 * @param rssi Signal strength of the received packet
 * @param snr Signal-to-noise ratio of the received packet
 */
typedef void (*BinaryPacketCallback)(uint8_t* data, size_t length, float rssi, float snr);

// Global variables for packet subscription system
extern PacketCallback _packetCallback;
extern BinaryPacketCallback _binaryPacketCallback;
extern volatile bool _packetReceived;
extern String _packetData;
extern uint8_t _binaryPacketBuffer[256]; // Buffer for binary packet data
extern size_t _binaryPacketLength;
extern float _packetRssi;
extern float _packetSnr;

// Packet handling functions
/**
 * @brief Internal callback handler for LoRa interrupts
 * (Not called directly by user code)
 */
void _handleLoRaRx();

/**
 * @brief Subscribe to LoRa packet reception events (string and/or binary)
 * @param packetCallback Function for string packet reception (optional)
 * @param binaryPacketCallback Function for binary packet reception (optional)
 * @return true if subscription was successful
 */
bool SensorSentinel_subscribe(PacketCallback packetCallback, BinaryPacketCallback binaryPacketCallback);

/**
 * @brief Unsubscribe from LoRa packet reception events (string and/or binary)
 * @param packetCallback true to unsubscribe from string packets
 * @param binaryPacketCallback true to unsubscribe from binary packets
 * @return true if unsubscription was successful
 */
bool SensorSentinel_unsubscribe(bool packetCallback, bool binaryPacketCallback);

/**
 * @brief Process any received packets in the main loop
 * This should be called in every iteration of loop()
 */
void SensorSentinel_process_packets();

#endif // SensorSentinel_RadioLib_HELPER_H