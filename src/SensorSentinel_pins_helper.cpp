/**
 * @file Heltec_Pins.cpp
 * @brief Implementation of pin-related functions for Heltec boards
 */

#include "Heltec_Pins.h"
#include <Arduino.h>

// Actual arrays for pin access
const uint8_t heltec_analog_pins[HELTEC_ANALOG_COUNT] = HELTEC_ANALOG_PINS;
const uint8_t heltec_boolean_pins[HELTEC_BOOLEAN_COUNT] = HELTEC_BOOLEAN_PINS;

/**
 * @brief Gets an available analog pin for the current board
 * 
 * @param index Index of the pin (0 to HELTEC_ANALOG_COUNT-1)
 * @return GPIO pin number or -1 if index is out of bounds
 */
int8_t heltec_get_analog_pin(uint8_t index) {
  if (index < HELTEC_ANALOG_COUNT) {
    return heltec_analog_pins[index];
  }
  return -1;
}

/**
 * @brief Gets an available digital pin for the current board
 * 
 * @param index Index of the pin (0 to HELTEC_BOOLEAN_COUNT-1) 
 * @return GPIO pin number or -1 if index is out of bounds
 */
int8_t heltec_get_boolean_pin(uint8_t index) {
  if (index < HELTEC_BOOLEAN_COUNT) {
    return heltec_boolean_pins[index];
  }
  return -1;
}

/**
 * @brief Gets the number of available analog pins
 * 
 * @return Number of available analog pins
 */
uint8_t heltec_get_analog_count() {
  return HELTEC_ANALOG_COUNT;
}

/**
 * @brief Gets the number of available digital pins
 * 
 * @return Number of available digital pins
 */
uint8_t heltec_get_boolean_count() {
  return HELTEC_BOOLEAN_COUNT;
}

/**
 * @brief Reads an analog value from one of the available pins
 * 
 * @param index Index of the pin (0 to HELTEC_ANALOG_COUNT-1)
 * @return Analog value (0-4095) or -1 if index is out of bounds
 */
int16_t heltec_read_analog(uint8_t index) {
  int8_t pin = heltec_get_analog_pin(index);
  if (pin >= 0) {
    return analogRead(pin);
  }
  return -1;
}

/**
 * @brief Reads a digital value from one of the available pins
 * 
 * @param index Index of the pin (0 to HELTEC_BOOLEAN_COUNT-1)
 * @return Digital value (0 or 1) or -1 if index is out of bounds
 */
int8_t heltec_read_boolean(uint8_t index) {
  int8_t pin = heltec_get_boolean_pin(index);
  if (pin >= 0) {
    pinMode(pin, INPUT);
    return digitalRead(pin);
  }
  return -1;
}

/**
 * @brief Writes a digital value to one of the available pins
 * 
 * @param index Index of the pin (0 to HELTEC_BOOLEAN_COUNT-1)
 * @param value Value to write (HIGH or LOW)
 * @return 0 if successful, -1 if index is out of bounds
 */
int8_t heltec_write_boolean(uint8_t index, uint8_t value) {
  int8_t pin = heltec_get_boolean_pin(index);
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
uint8_t heltec_read_all_analog(uint16_t* values, uint8_t maxValues) {
  uint8_t count = maxValues < HELTEC_ANALOG_COUNT ? maxValues : HELTEC_ANALOG_COUNT;  
  for (uint8_t i = 0; i < count; i++) {
    values[i] = heltec_read_analog(i);
  }
  return count;
}

/**
 * @brief Reads all available analog pins into an array, filling remaining slots with zeros
 * 
 * @param values Pointer to array where values will be stored
 * @param arraySize Size of the provided array
 */
void heltec_read_analog_with_padding(uint16_t* values, uint8_t arraySize) {
  // Read available values
  uint8_t count = heltec_read_all_analog(values, arraySize);
  
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
uint8_t heltec_read_boolean_byte(uint8_t maxBits) {
  uint8_t result = 0;
  uint8_t count = maxBits < HELTEC_BOOLEAN_COUNT ? maxBits : HELTEC_BOOLEAN_COUNT; 
  for (uint8_t i = 0; i < count; i++) {
    result |= (heltec_read_boolean(i) & 0x01) << i;
  }
  
  return result;
}

/**
 * @brief Reads all sensor pins into a single structure
 * 
 * @param readings Pointer to structure where readings will be stored
 */
void heltec_read_all_pins(heltec_pin_readings_t* readings) {
  // Read analog values with padding
  heltec_read_analog_with_padding(readings->analog, 4);
  
  // Read boolean values
  readings->boolean = heltec_read_boolean_byte(8);
}

/**
 * @brief Print information about available pins to Serial
 */
void heltec_print_available_pins() {
  Serial.println("Available pins:");
  
  // Print analog pins
  Serial.printf("Analog (%d): ", HELTEC_ANALOG_COUNT);
  for (uint8_t i = 0; i < HELTEC_ANALOG_COUNT; i++) {
    Serial.printf("GPIO%d", heltec_analog_pins[i]);
    if (i < HELTEC_ANALOG_COUNT - 1) Serial.printf(", ");
  }
  Serial.println();
  
  // Print digital pins
  Serial.printf("Digital (%d): ", HELTEC_BOOLEAN_COUNT);
  for (uint8_t i = 0; i < HELTEC_BOOLEAN_COUNT; i++) {
    Serial.printf("GPIO%d", heltec_boolean_pins[i]);
    if (i < HELTEC_BOOLEAN_COUNT - 1) Serial.printf(", ");
  }
  Serial.println();
}