/**
 * @file SensorSentinel_GNSS_helper.cpp
 * @brief Helper functions for the TinyGPS library
 */
#include <SensorSentinel_GNSS_helper.h>

// ====== GNSS functions ======  
#ifdef GNSS  

  /**  
   * @brief Puts the GNSS module to sleep  
   */  
  void SensorSentinel_gnss_sleep() {  
    gpsSerial.end();  
    pinMode(GNSS_TX, INPUT);  
    pinMode(GNSS_RX, INPUT);  
  }  

  /**  
   * @brief Updates GNSS data if available  
   * @return true if new data was processed  
   */  
  bool SensorSentinel_gnss_update() {  
    if (!gpsSerial) return false;  
    if (digitalRead(VEXT) == HIGH) return false; // Check VEXT state (LOW=on)  
    
    while (gpsSerial.available()) {  
      if (gps.encode(gpsSerial.read())) {  
        return true; // New data available  
      }  
    }  
    return false;  
  }  
#endif  
