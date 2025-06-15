/**
 * @file SensorSentinel_Pins_Helper.cpp
 * @brief Implementation of pin-related functions for Heltec boards
 */

#include "SensorSentinel_pins_helper.h"
#include "heltec_unofficial_revised.h"
#include <Arduino.h>

// Actual arrays for pin access
const uint8_t SensorSentinel_analog_pins[SensorSentinel_ANALOG_COUNT] = SensorSentinel_ANALOG_PINS;
const uint8_t SensorSentinel_boolean_pins[SensorSentinel_BOOLEAN_COUNT] = SensorSentinel_BOOLEAN_PINS;

/**
 * @brief Gets an available analog pin for the current board
 * 
 * @param index Index of the pin (0 to SensorSentinel_ANALOG_COUNT-1)
 * @return GPIO pin number or -1 if index is out of bounds
 */
int8_t SensorSentinel_get_analog_pin(uint8_t index) {
  if (index < SensorSentinel_ANALOG_COUNT) {
    return SensorSentinel_analog_pins[index];
  }
  return -1;
}

/**
 * @brief Gets an available digital pin for the current board
 * 
 * @param index Index of the pin (0 to SensorSentinel_BOOLEAN_COUNT-1) 
 * @return GPIO pin number or -1 if index is out of bounds
 */
int8_t SensorSentinel_get_boolean_pin(uint8_t index) {
  if (index < SensorSentinel_BOOLEAN_COUNT) {
    return SensorSentinel_boolean_pins[index];
  }
  return -1;
}

/**
 * @brief Gets the number of available analog pins
 * 
 * @return Number of available analog pins
 */
uint8_t SensorSentinel_get_analog_count() {
  return SensorSentinel_ANALOG_COUNT;
}

/**
 * @brief Gets the number of available digital pins
 * 
 * @return Number of available digital pins
 */
uint8_t SensorSentinel_get_boolean_count() {
  return SensorSentinel_BOOLEAN_COUNT;
}

/**
 * @brief Reads an analog value from one of the available pins
 * 
 * @param index Index of the pin (0 to SensorSentinel_ANALOG_COUNT-1)
 * @return Analog value (0-4095) or -1 if index is out of bounds
 */
int16_t SensorSentinel_read_analog(uint8_t index) {
  int8_t pin = SensorSentinel_get_analog_pin(index);
  if (pin >= 0) {
    return analogRead(pin);
  }
  return -1;
}

/**
 * @brief Reads a digital value from one of the available pins
 * 
 * @param index Index of the pin (0 to SensorSentinel_BOOLEAN_COUNT-1)
 * @return Digital value (0 or 1) or -1 if index is out of bounds
 */
int8_t SensorSentinel_read_boolean(uint8_t index) {
  int8_t pin = SensorSentinel_get_boolean_pin(index);
  if (pin >= 0) {
    pinMode(pin, INPUT);
    return digitalRead(pin);
  }
  return -1;
}

/**
 * @brief Writes a digital value to one of the available pins
 * 
 * @param index Index of the pin (0 to SensorSentinel_BOOLEAN_COUNT-1)
 * @param value Value to write (HIGH or LOW)
 * @return 0 if successful, -1 if index is out of bounds
 */
int8_t SensorSentinel_write_boolean(uint8_t index, uint8_t value) {
  int8_t pin = SensorSentinel_get_boolean_pin(index);
  if (pin >= 0) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, value);
    return 0;
  }
  return -1;
}

/**
 * @brief Reads all analog pins into an array
 * 
 * @param values Pointer to array where values will be stored
 * @param maxValues Maximum number of values to read
 * @return Number of values read
 */
uint8_t SensorSentinel_read_all_analog(uint16_t* values, uint8_t maxValues) {
  uint8_t count = maxValues < SensorSentinel_ANALOG_COUNT ? maxValues : SensorSentinel_ANALOG_COUNT;  
  for (uint8_t i = 0; i < count; i++) {
    values[i] = SensorSentinel_read_analog(i);
  }
  return count;
}

/**
 * @brief Reads all available analog pins into an array, filling remaining slots with zeros
 * 
 * @param values Pointer to array where values will be stored
 * @param arraySize Size of the provided array
 */
void SensorSentinel_read_analog_with_padding(uint16_t* values, uint8_t arraySize) {
  // Read available values
  uint8_t count = SensorSentinel_read_all_analog(values, arraySize);
  
  // Fill remaining slots with zeros
  for (uint8_t i = count; i < arraySize; i++) {
    values[i] = 0;
  }
}

/**
 * @brief Reads all boolean pins and packs them into a byte
 * 
 * @param maxBits Maximum number of bits to read (1-8)
 * @return Byte with each bit representing a pin state
 */
uint8_t SensorSentinel_read_boolean_byte(uint8_t maxBits) {
  uint8_t result = 0;
  uint8_t count = maxBits < SensorSentinel_BOOLEAN_COUNT ? maxBits : SensorSentinel_BOOLEAN_COUNT; 
  for (uint8_t i = 0; i < count; i++) {
    result |= (SensorSentinel_read_boolean(i) & 0x01) << i;
  }
  
  return result;
}

/**
 * @brief Reads all sensor pins into a single structure
 * 
 * @param readings Pointer to structure where readings will be stored
 */
void SensorSentinel_read_all_pins(SensorSentinel_pin_readings_t* readings) {
  // Read analog values with padding
  SensorSentinel_read_analog_with_padding(readings->analog, 4);
  
  // Read boolean values
  readings->boolean = SensorSentinel_read_boolean_byte(8);
}

/**
 * @brief Print information about available pins to Serial
 */
void SensorSentinel_print_available_pins() {
  Serial.println("Available pins:");
  
  // Print analog pins
  Serial.printf("Analog (%d): ", SensorSentinel_ANALOG_COUNT);
  for (uint8_t i = 0; i < SensorSentinel_ANALOG_COUNT; i++) {
    Serial.printf("GPIO%d", SensorSentinel_analog_pins[i]);
    if (i < SensorSentinel_ANALOG_COUNT - 1) Serial.printf(", ");
  }
  Serial.println();
  
  // Print digital pins
  Serial.printf("Digital (%d): ", SensorSentinel_BOOLEAN_COUNT);
  for (uint8_t i = 0; i < SensorSentinel_BOOLEAN_COUNT; i++) {
    Serial.printf("GPIO%d", SensorSentinel_boolean_pins[i]);
    if (i < SensorSentinel_BOOLEAN_COUNT - 1) Serial.printf(", ");
  }
  Serial.println();
}