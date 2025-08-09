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
 * @brief Reads an analog value from one of the available pins
 * 
 * @param index Index of the pin (0 to SensorSentinel_ANALOG_COUNT-1)
 * @return Analog value (0-4095) or -1 if index is out of bounds
 */
int16_t SensorSentinel_read_analog(uint8_t index) {
    return (index < SensorSentinel_ANALOG_COUNT) ? 
           analogRead(SensorSentinel_analog_pins[index]) : -1;
}

/**
 * @brief Reads a digital value from one of the available pins
 * 
 * @param index Index of the pin (0 to SensorSentinel_BOOLEAN_COUNT-1)
 * @return Digital value (0 or 1) or -1 if index is out of bounds
 */
int8_t SensorSentinel_read_boolean(uint8_t index) {
    if (index >= SensorSentinel_BOOLEAN_COUNT) return -1;
    
    uint8_t pin = SensorSentinel_boolean_pins[index];
    pinMode(pin, INPUT);
    return digitalRead(pin);
}

/**
 * @brief Writes a digital value to one of the available pins
 * 
 * @param index Index of the pin (0 to SensorSentinel_BOOLEAN_COUNT-1)
 * @param value Value to write (HIGH or LOW)
 * @return 0 if successful, -1 if index is out of bounds
 */
int8_t SensorSentinel_write_boolean(uint8_t index, uint8_t value) {
    if (index >= SensorSentinel_BOOLEAN_COUNT) return -1;
    
    uint8_t pin = SensorSentinel_boolean_pins[index];
    pinMode(pin, OUTPUT);
    digitalWrite(pin, value);
    return 0;
}

/**
 * @brief Reads all analog pins into an array
 * 
 * @param values Pointer to array where values will be stored
 * @param maxValues Maximum number of values to read
 * @return Number of values read
 */
void SensorSentinel_read_all_analog(uint16_t* values, uint8_t arraySize) {
    uint8_t count = (arraySize < SensorSentinel_ANALOG_COUNT) ? arraySize : SensorSentinel_ANALOG_COUNT;
    
    // Read available pins
    for (uint8_t i = 0; i < count; i++) {
        values[i] = analogRead(SensorSentinel_analog_pins[i]);
    }
    
    // Zero fill remaining slots
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
uint8_t SensorSentinel_read_all_boolean() {
    uint8_t result = 0;
    uint8_t count = (8 < SensorSentinel_BOOLEAN_COUNT) ? 8 : SensorSentinel_BOOLEAN_COUNT;
    
    for (uint8_t i = 0; i < count; i++) {
        uint8_t pin = SensorSentinel_boolean_pins[i];
        pinMode(pin, INPUT);
        if (digitalRead(pin)) {
            result |= (1 << i);
        }
    }
    return result;
}

/**
 * @brief Reads all sensor pins into a single structure
 * 
 * @param readings Pointer to structure where readings will be stored
 */
void SensorSentinel_read_all_pins(SensorSentinel_pin_readings_t* readings) {
    SensorSentinel_read_all_analog(readings->analog, 4);
    readings->boolean = SensorSentinel_read_all_boolean();
}

/**
 * @brief Print information about available pins to Serial
 */
void SensorSentinel_print_available_pins() {
    Serial.print("Analog pins: ");
    for (uint8_t i = 0; i < SensorSentinel_ANALOG_COUNT; i++) {
        Serial.printf("GPIO%d%s", SensorSentinel_analog_pins[i], 
                     (i < SensorSentinel_ANALOG_COUNT - 1) ? ", " : "\n");
    }
    
    Serial.print("Digital pins: ");
    for (uint8_t i = 0; i < SensorSentinel_BOOLEAN_COUNT; i++) {
        Serial.printf("GPIO%d%s", SensorSentinel_boolean_pins[i], 
                     (i < SensorSentinel_BOOLEAN_COUNT - 1) ? ", " : "\n");
    }
}