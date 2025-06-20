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
 * @brief Subscribe to LoRa packet reception events  
 * @param callback Function to be called when a packet is received  
 * @return true if subscription was successful  
 */  
bool SensorSentinel_subscribe_packets(PacketCallback callback) {  
  #ifndef NO_RADIOLIB  
    // Validate input  
    if (callback == NULL) {  
      Serial.println("Error: NULL callback provided");  
      return false;  
    }  
    
    // Store the callback  
    _packetCallback = callback;  
    
    // Reset received flag to ensure clean state  
    _packetReceived = false;  
    
    // Clear any previous action first  
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
 * @brief Subscribe to LoRa binary packet reception events  
 * @param callback Function to be called when a binary packet is received  
 * @return true if subscription was successful  
 */  
bool SensorSentinel_subscribe_binary_packets(BinaryPacketCallback callback) {  
  #ifndef NO_RADIOLIB  
    // Validate input  
    if (callback == NULL) {  
      Serial.println("Error: NULL callback provided");  
      return false;  
    }  
    
    // Store the callback  
    _binaryPacketCallback = callback;  
    
    // Reset received flag to ensure clean state  
    _packetReceived = false;  
    
    // Clear any previous action first  
    radio.clearDio1Action();  
    delay(10);  
    
    // Set up the new action  
    radio.setDio1Action(_handleLoRaRx);  
    delay(10);  
    
    // Start receive  
    int state = radio.startReceive();  
    delay(10);  

    if (state == RADIOLIB_ERR_NONE) {  
      Serial.println("LoRa binary packet subscription active");  
      return true;  
    } else {  
      Serial.printf("Failed to start radio receiver: %d\n", state);  
      return false;  
    }  
  #else  
    Serial.println("Radio not available - binary packet subscription failed");  
    return false;  
  #endif  
}  

/**  
 * @brief Unsubscribe from LoRa packet reception events  
 * @return true if unsubscription was successful  
 */  
bool SensorSentinel_unsubscribe_packets() {  
  #ifndef NO_RADIOLIB  
    // Clear the callback  
    _packetCallback = NULL;  
    
    // If neither callback is active, fully disable reception  
    if (_binaryPacketCallback == NULL) {  
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
 * @brief Unsubscribe from LoRa binary packet reception events  
 * @return true if unsubscription was successful  
 */  
bool SensorSentinel_unsubscribe_binary_packets() {  
  #ifndef NO_RADIOLIB  
    // Clear the callback  
    _binaryPacketCallback = NULL;  
    
    // If neither callback is active, fully disable reception  
    if (_packetCallback == NULL) {  
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
  #ifndef NO_RADIOLIB  
    if (_packetReceived) {  
      // Clear flag immediately to prevent race conditions  
      _packetReceived = false;  
      
      // Temporarily disable interrupt to prevent recursion  
      radio.clearDio1Action();  
      
      // Check if we have any callbacks registered  
      bool hasCallbacks = (_packetCallback != NULL || _binaryPacketCallback != NULL);  
      
      if (hasCallbacks) {  
        // Get signal quality information (needed for both callback types)  
        _packetRssi = radio.getRSSI();  
        _packetSnr = radio.getSNR();  
        
        // Handle string packet (for backward compatibility)  
        if (_packetCallback) {  
          _packetData = "";  
          int state = radio.readData(_packetData);  
          
          if (state == RADIOLIB_ERR_NONE) {  
            _packetCallback(_packetData, _packetRssi, _packetSnr);  
          } else {  
            Serial.printf("Error reading LoRa packet as string: %d\n", state);  
          }  
        }  
        
        // Handle binary packet  
        if (_binaryPacketCallback) {  
            // Use RadioLib's API correctly
            int state = radio.readData(_binaryPacketBuffer, sizeof(_binaryPacketBuffer));
            
            if (state == RADIOLIB_ERR_NONE) {
                // Get the actual packet length
                _binaryPacketLength = radio.getPacketLength();
                
                if (_binaryPacketLength > 0) {
                _binaryPacketCallback(_binaryPacketBuffer, _binaryPacketLength, _packetRssi, _packetSnr);
                } else {
                Serial.println("Received empty binary packet");
                    }
            } else {
                Serial.printf("Error reading LoRa packet as binary: %d\n", state);
            }
        } 
      }  
      
      // Make sure radio is in a clean state before restarting reception  
      radio.standby();  
      delay(10);  
      
      // Only restart reception if we still have a callback (user might have unsubscribed in callback)  
      if (_packetCallback || _binaryPacketCallback) {  
        // Restart reception  
        radio.startReceive();  
        delay(10);  
        
        // Re-enable interrupt handler AFTER reception has started  
        radio.setDio1Action(_handleLoRaRx);  
      }  
    }  
  #endif  
}  