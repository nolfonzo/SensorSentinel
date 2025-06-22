/**
 * @file SensorSentinel_GNSS_helper.h
 * @brief Helper functions for the TinyGPS library
 */

#ifndef SensorSentinel_GNSS_HELPER_H
#define SensorSentinel_GNSS_HELPER_H

#include <heltec_unofficial_revised.h>

#include <TinyGPSPlus.h>
// GNSS global variables
extern TinyGPSPlus gps;
extern HardwareSerial gpsSerial;

/**
 * @brief Initializes the GNSS module
 */
void SensorSentinel_gnss_begin();

/**
 * @brief Puts the GNSS module to sleep
 */
void SensorSentinel_gnss_sleep();

/**
 * @brief Updates GNSS data if available
 * @return True if new data was processed
 */
bool SensorSentinel_gnss_update();

#endif // SensorSentinel_GNSS_HELPER_H