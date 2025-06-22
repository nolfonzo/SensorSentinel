/**
 * @file SensorSentinel_RadioLib_helper.cpp
 * @brief Implementation of RadioLib helper functions including subscription services 
 */

#include "SensorSentinel_RadioLib_helper.h"

// Packet subscription system  
PacketCallback _packetCallback = NULL;  
BinaryPacketCallback _binaryPacketCallback = NULL;  
volatile bool _packetReceived = false;  
String _packetData;  
uint8_t _binaryPacketBuffer[256]; // Buffer for binary packet data  
size_t _binaryPacketLength = 0;  
float _packetRssi;  
float _packetSnr;  

// ====== Internal helper functions ======  
// Interrupt handler for packet reception  
void _handleLoRaRx() {  
  _packetReceived = true;  
}  

// ====== Radio functions ======  

/**
 * @brief Subscribe to LoRa packet reception events (string and/or binary)
 * @param packetCallback Function for string packet reception (optional)
 * @param binaryPacketCallback Function for binary packet reception (optional)
 * @return true if subscription was successful
 */
bool SensorSentinel_subscribe(PacketCallback packetCallback, BinaryPacketCallback binaryPacketCallback) {
#ifndef HELTEC_NO_RADIOLIB
    // Validate input: at least one callback must be provided
    if (packetCallback == NULL && binaryPacketCallback == NULL) {
        Serial.println("Error: At least one callback must be provided");
        return false;
    }

    // Store the callbacks if provided
    if (packetCallback != NULL) {
        _packetCallback = packetCallback;
    }
    if (binaryPacketCallback != NULL) {
        _binaryPacketCallback = binaryPacketCallback;
    }

    // Reset received flag to ensure clean state
    _packetReceived = false;

    // Clear any previous action
    radio.clearDio1Action();
    delay(10);

    // Set up the new action
    radio.setDio1Action(_handleLoRaRx);
    delay(10);

    // Start receive
    int state = radio.startReceive();
    delay(10);

    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("LoRa packet subscription active");
        return true;
    } else {
        Serial.printf("Failed to start radio receiver: %d\n", state);
        return false;
    }
#else
    Serial.println("Radio not available - packet subscription failed");
    return false;
#endif
}

/**
 * @brief Unsubscribe from LoRa packet reception events (string and/or binary)
 * @param packetCallback true to unsubscribe from string packets
 * @param binaryPacketCallback true to unsubscribe from binary packets
 * @return true if unsubscription was successful
 */
bool SensorSentinel_unsubscribe(bool packetCallback, bool binaryPacketCallback) {
#ifndef HELTEC_NO_RADIOLIB
    // Clear the specified callbacks
    if (packetCallback) {
        _packetCallback = NULL;
    }
    if (binaryPacketCallback) {
        _binaryPacketCallback = NULL;
    }

    // If neither callback is active, fully disable reception
    if (_packetCallback == NULL && _binaryPacketCallback == NULL) {
        // Reset received flag to ensure clean state
        _packetReceived = false;

        // Clear the interrupt
        radio.clearDio1Action();
        delay(10);

        // Put radio in standby mode
        int state = radio.standby();
        delay(10);

        if (state == RADIOLIB_ERR_NONE) {
            Serial.println("LoRa packet subscription disabled");
            return true;
        } else {
            Serial.printf("Failed to put radio in standby: %d\n", state);
            return false;
        }
    }
    return true;
#else
    Serial.println("Radio not available - unsubscription not needed");
    return false;
#endif
}

/**
 * @brief Process any received packets in the main loop
 */
void SensorSentinel_process_packets() {
#ifndef HELTEC_NO_RADIOLIB
    if (_packetReceived) {
        _packetReceived = false;
        radio.clearDio1Action();
        
        bool hasCallbacks = (_packetCallback != NULL || _binaryPacketCallback != NULL);
        
        if (hasCallbacks) {
            _packetRssi = radio.getRSSI();
            _packetSnr = radio.getSNR();
            
            // Read all packets as binary
            int state = radio.readData(_binaryPacketBuffer, sizeof(_binaryPacketBuffer));
            _binaryPacketLength = radio.getPacketLength();
            
            if (state == RADIOLIB_ERR_NONE && _binaryPacketLength > 0) {
                // Handle string callback
                if (_packetCallback) {
                    // Convert buffer to String using length-based constructor
                    _packetData = String((char*)_binaryPacketBuffer, _binaryPacketLength);
                    _packetCallback(_packetData, _packetRssi, _packetSnr);
                }
                
                // Handle binary callback
                if (_binaryPacketCallback) {
                    _binaryPacketCallback(_binaryPacketBuffer, _binaryPacketLength, _packetRssi, _packetSnr);
                }
            } else {
                Serial.printf("Error reading LoRa packet or empty packet: %d\n", state);
            }
        }
        
        radio.standby();
        delay(10);
        
        if (_packetCallback || _binaryPacketCallback) {
            radio.startReceive();
            delay(10);
            radio.setDio1Action(_handleLoRaRx);
        }
    }
#endif
}