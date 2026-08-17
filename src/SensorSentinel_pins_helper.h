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
#define SensorSentinel_PINS_HELPER_H

#include <Arduino.h>

// Common constants for all boards
#define SensorSentinel_GPIO_ANALOG_COUNT   4  // 4 Physical ADC Channels
#define SensorSentinel_GPIO_DIGITAL_COUNT  16 // 16 Binary Inputs
#define SensorSentinel_BUS_SLOTS_COUNT     8  // 8 Digital Bus / I2C Telemetry Slots

#define SensorSentinel_ANALOG_COUNT        SensorSentinel_GPIO_ANALOG_COUNT
#define SensorSentinel_BOOLEAN_COUNT       SensorSentinel_GPIO_DIGITAL_COUNT

#if defined(WOKWI)
  // Wokwi available pins
  #define SensorSentinel_ANALOG_PINS     {32, 33, 34, 35}
  #define SensorSentinel_BOOLEAN_PINS    {4, 5, 6, 36, 36, 37, 38, 39, 0, 0, 0, 0, 0, 0, 0, 0}

// Available pins for external use (analog and digital) - board specific
#elif defined(ARDUINO_heltec_wireless_tracker)
  // Wireless Tracker available pins (ESP32-S3)
  #define SensorSentinel_ANALOG_PINS     {4, 5, 6, 7}
  #define SensorSentinel_BOOLEAN_PINS    {26, 33, 34, 37, 45, 46, 47, 48, 0, 0, 0, 0, 0, 0, 0, 0}

#elif defined(ARDUINO_heltec_wifi_lora_32_V3) || defined(BOARD_HELTEC_V3_2)
  // WiFi LoRa 32 V3/V3.2 available pins (ESP32-S3)
  #define SensorSentinel_ANALOG_PINS     {1, 2, 3, 4}
  #define SensorSentinel_BOOLEAN_PINS    {33, 34, 35, 39, 40, 41, 42, 46, 0, 0, 0, 0, 0, 0, 0, 0}

#elif defined(ARDUINO_heltec_wireless_stick) || defined(ARDUINO_heltec_wireless_stick_lite)
  // Wireless Stick/Stick Lite available pins
  #define SensorSentinel_ANALOG_PINS     {1, 2, 3, 4}
  #define SensorSentinel_BOOLEAN_PINS    {5, 6, 7, 33, 34, 35, 36, 37, 0, 0, 0, 0, 0, 0, 0, 0}

#else
  // Default pins
  #define SensorSentinel_ANALOG_PINS     {1, 2, 3, 4}
  #define SensorSentinel_BOOLEAN_PINS    {33, 34, 35, 39, 40, 41, 42, 46, 0, 0, 0, 0, 0, 0, 0, 0}
#endif

/**
 * @brief Struct for holding pin readings for packet transmission
 */
typedef struct {
  uint16_t digitalBitmask;                               // 16 digital boolean flags
  uint16_t gpioAnalog[SensorSentinel_GPIO_ANALOG_COUNT]; // 4 physical ADC channels
  uint16_t busTelemetry[SensorSentinel_BUS_SLOTS_COUNT]; // 8 digital bus / I2C channels
  uint8_t  discoveredDevices;                            // Count of auto-detected I2C devices
} __attribute__((packed)) SensorSentinel_pin_readings_t;

// Pin arrays (defined in SensorSentinel_pins_helper.cpp)
extern const uint8_t SensorSentinel_analog_pins[SensorSentinel_ANALOG_COUNT];
extern const uint8_t SensorSentinel_boolean_pins[SensorSentinel_BOOLEAN_COUNT];

// Function declarations
int16_t SensorSentinel_read_analog(uint8_t index);
int8_t SensorSentinel_read_boolean(uint8_t index);
int8_t SensorSentinel_write_boolean(uint8_t index, uint8_t value);
void SensorSentinel_read_all_analog(uint16_t* values, uint8_t arraySize);
uint16_t SensorSentinel_read_all_boolean();
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
 * Which digital slots are actually usable for a simple switch to GND
 * (verified on hardware 2026-08-17, V3.2):
 * - D0/D1 (33,34) and D5/D6 (41,42): swept by I2C auto-discovery as candidate
 *   SDA/SCL pairs, so they can be driven during a scan. Avoid.
 * - D2 (35): also drives LED Write Ctrl. Avoid.
 * - D3 (39): ESP32-S3 JTAG MTCK. Tested and it does NOT respond to being tied
 *   to GND despite INPUT_PULLUP - it reads high in both states. Avoid.
 * - D7 (46): ESP32-S3 strapping pin. Avoid.
 * - D4 (40): free and verified working. Use this one first.
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