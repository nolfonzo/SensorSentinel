/**
 * @file SensorSentinel_i2c_helper.h
 * @brief Dynamic I2C auto-discovery and sensor manager for Heltec boards.
 *
 * Supports plug-and-play detection and reading of I2C sensors without reflashing:
 * - DFRobot SEN0291 / TI INA219 (Voltage, Current, Power) at 0x40-0x45
 * - TI INA226 (High-Precision Voltage & Current) at 0x40-0x4F
 * - ADS1115 (4-Channel 16-bit ADC) at 0x48-0x4B
 * - BME280 / BMP280 (Temp, Humidity, Pressure) at 0x76, 0x77
 * - AHT20 / DHT20 (Temp & Humidity) at 0x38
 * - BH1750 (Ambient Light Lux) at 0x23
 */

#ifndef SENSORSENTINEL_I2C_HELPER_H
#define SENSORSENTINEL_I2C_HELPER_H

#include <Arduino.h>
#include <Wire.h>

// Maximum number of I2C devices to manage concurrently
#define SENSORSENTINEL_MAX_I2C_DEVICES 8
#define SENSORSENTINEL_I2C_READ_TIMEOUT_MS 50

// Sensor types supported by the driver registry
typedef enum {
    I2C_SENSOR_UNKNOWN = 0,
    I2C_SENSOR_INA219,      // SEN0291 / INA219 (Voltage mV, Current mA, Power mW)
    I2C_SENSOR_INA226,      // INA226 (Voltage mV, Current mA, Power mW)
    I2C_SENSOR_ADS1115,     // 4-ch 16-bit ADC (Ch0-Ch3 mV)
    I2C_SENSOR_BME280,      // Temp 0.1C, Humidity 0.1%, Pressure hPa
    I2C_SENSOR_BMP280,      // Temp 0.1C, Pressure hPa
    I2C_SENSOR_AHT20,       // Temp 0.1C, Humidity 0.1%
    I2C_SENSOR_BH1750,      // Lux (0-65535)
    I2C_SENSOR_GENERIC      // Raw register responder
} i2c_sensor_type_t;

// Structure describing a discovered I2C sensor
typedef struct {
    uint8_t address;            // 7-bit I2C address (0x08 - 0x77)
    i2c_sensor_type_t type;     // Identified sensor type
    char name[16];              // Human readable name (e.g. "SEN0291-Volt")
    bool active;                // True if initialized and responding
    uint8_t startSlot;          // First telemetry payload slot assigned (0-7)
    uint8_t numSlots;           // Number of slots occupied
} i2c_device_desc_t;

// Function declarations
void SensorSentinel_i2c_init();
void SensorSentinel_i2c_power_rail(bool on);
bool SensorSentinel_i2c_bus_recovery(int sdaPin, int sclPin);
uint8_t SensorSentinel_i2c_auto_discover(bool forceScan = false);
void SensorSentinel_i2c_read_all(uint16_t* sensorValues, uint8_t maxSlots);

// Configuration getters/setters (persisted in NVS)
int SensorSentinel_i2c_get_sda_pin();
int SensorSentinel_i2c_get_scl_pin();
void SensorSentinel_i2c_set_pins(int sda, int scl);

// Diagnostics helper
void SensorSentinel_i2c_print_discovered();
const char* SensorSentinel_i2c_type_to_string(i2c_sensor_type_t type);
uint8_t SensorSentinel_i2c_get_device_count();
const i2c_device_desc_t* SensorSentinel_i2c_get_device(uint8_t index);

#endif // SENSORSENTINEL_I2C_HELPER_H
