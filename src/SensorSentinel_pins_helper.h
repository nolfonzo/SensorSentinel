/**
 * @file SensorSentinel_Pins_Helper.h
 * @brief Pin definitions and structures for Heltec board variants
 * 
 * Defines the available pins for external connections on various Heltec boards:
 * - WiFi LoRa 32 V3/V3.2
 * - Wireless Tracker
 * - Wireless Stick/Stick Lite
 * 
 * This file is based on official Heltec datasheets and testing.
 8 
 */

#ifndef SensorSentinel_PINS_HELPER_H
#define SensorSentinel_PINS__HELPER_H

#include <Arduino.h>

// Common constants for all boards
#define SensorSentinel_ANALOG_COUNT    4  // All boards have 4 analog pins
#define SensorSentinel_BOOLEAN_COUNT   8  // All boards have 8 boolean pins

// Available pins for external use (analog and digital) - board specific
#if defined(ARDUINO_heltec_wireless_tracker)
  // Wireless Tracker available pins (ESP32-S3)
  // Based on Wireless Tracker datasheet
  #define SensorSentinel_ANALOG_PINS     {4, 5, 6, 7}
  #define SensorSentinel_BOOLEAN_PINS    {39, 40, 41, 42, 43, 44, 45, 46}

#elif defined(ARDUINO_heltec_wifi_32_lora_V3) || defined(BOARD_HELTEC_V3_2)
  // WiFi LoRa 32 V3/V3.2 available pins (ESP32-S3)
  // Based on WiFi LoRa 32 V3 datasheet
  #define SensorSentinel_ANALOG_PINS     {1, 2, 3, 4}
  #define SensorSentinel_BOOLEAN_PINS    {33, 34, 35, 39, 40, 41, 42, 46}

#elif defined(ARDUINO_SensorSentinel_wireless_stick) || defined(ARDUINO_SensorSentinel_wireless_stick_lite)
  // Wireless Stick/Stick Lite available pins
  #define SensorSentinel_ANALOG_PINS     {1, 2, 3, 4}
  #define SensorSentinel_BOOLEAN_PINS    {5, 6, 7, 33, 34, 35, 36, 37}

#else
  // Default pins (may not work on all boards)
  #define SensorSentinel_ANALOG_PINS     {1, 2, 3, 4}
  #define SensorSentinel_BOOLEAN_PINS    {33, 34, 35, 39, 40, 41, 42, 46}
#endif

/**
 * @brief Struct for holding pin readings for packet transmission
 */
typedef struct {
  uint16_t analog[4];   // 4 analog values
  uint8_t boolean;      // 8 boolean values packed as bits
} SensorSentinel_pin_readings_t;

// Function declarations
int8_t SensorSentinel_get_analog_pin(uint8_t index);
int8_t SensorSentinel_get_boolean_pin(uint8_t index);
uint8_t SensorSentinel_get_analog_count();
uint8_t SensorSentinel_get_boolean_count();
int16_t SensorSentinel_read_analog(uint8_t index);
int8_t SensorSentinel_read_boolean(uint8_t index);
int8_t SensorSentinel_write_boolean(uint8_t index, uint8_t value);
uint8_t SensorSentinel_read_all_analog(uint16_t* values, uint8_t maxValues);
void SensorSentinel_read_analog_with_padding(uint16_t* values, uint8_t arraySize);
uint8_t SensorSentinel_read_boolean_byte(uint8_t maxBits = 8);
void SensorSentinel_read_all_pins(SensorSentinel_pin_readings_t* readings);
void SensorSentinel_print_available_pins();

/**
 * Pin Functions:
 * 
 * WiFi LoRa 32 V3/V3.2:
 * - GPIO1: ADC1_CH0, Used for battery voltage reading
 * - GPIO33, 34, 35: Available on Header J2
 * - GPIO39-42, 46: Available on Header J3
 * - Note: GPIO35 is also used for LED Write Ctrl
 * 
 * Wireless Tracker:
 * - GPIO4-7: ADC1 channels available on Header J3
 * - GPIO39-46: Digital pins available on Headers J2 and J3
 * - Note: GPIO1 is used for battery voltage reading
 * 
 * Wireless Stick/Stick Lite:
 * - GPIO1-4: ADC channels
 * - GPIO5-7, 33-37: Available for digital I/O
 */

#endif // SensorSentinel_PINS_HELPER_H