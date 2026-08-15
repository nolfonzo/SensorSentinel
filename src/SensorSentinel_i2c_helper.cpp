/**
 * @file SensorSentinel_i2c_helper.cpp
 * @brief Dynamic I2C auto-discovery and sensor driver implementation.
 */

#include "SensorSentinel_i2c_helper.h"
#include <Preferences.h>

#ifndef VEXT
#define VEXT GPIO_NUM_36
#endif

// Default I2C pins for Heltec V3 / V3.2
#define DEFAULT_I2C_SDA  1
#define DEFAULT_I2C_SCL  2

// RTC-preserved discovered device cache for fast deep sleep wakeups
RTC_DATA_ATTR static uint8_t _rtc_device_count = 0;
RTC_DATA_ATTR static i2c_device_desc_t _rtc_devices[SENSORSENTINEL_MAX_I2C_DEVICES];
RTC_DATA_ATTR static int _rtc_active_sda = DEFAULT_I2C_SDA;
RTC_DATA_ATTR static int _rtc_active_scl = DEFAULT_I2C_SCL;
RTC_DATA_ATTR static bool _rtc_discovery_valid = false;

static Preferences _i2cPrefs;
static bool _wireInitialized = false;

// ── Power rail control with settling delay ────────────────────────────────────
void SensorSentinel_i2c_power_rail(bool on) {
    pinMode(VEXT, OUTPUT);
    if (on) {
        // VEXT on Heltec boards is active LOW
        digitalWrite(VEXT, LOW);
        // Essential 15ms settling time for sensor decoupling capacitors & POR
        delay(15);
    } else {
        digitalWrite(VEXT, HIGH);
    }
}

// ── 9-Clock I2C bus recovery routine ──────────────────────────────────────────
// If a sensor was mid-transaction when the ESP32 reset or went to sleep,
// it may be holding SDA LOW. Pulsing SCL 9 times resets the slave state machine.
bool SensorSentinel_i2c_bus_recovery(int sdaPin, int sclPin) {
    if (sdaPin < 0 || sclPin < 0) return false;
    
    pinMode(sdaPin, INPUT_PULLUP);
    pinMode(sclPin, OUTPUT_OPEN_DRAIN);
    digitalWrite(sclPin, HIGH);
    delayMicroseconds(10);

    for (int i = 0; i < 9; i++) {
        digitalWrite(sclPin, LOW);
        delayMicroseconds(5);
        digitalWrite(sclPin, HIGH);
        delayMicroseconds(5);
        if (digitalRead(sdaPin) == HIGH) {
            break; // Bus released
        }
    }

    // Generate STOP condition
    pinMode(sdaPin, OUTPUT_OPEN_DRAIN);
    digitalWrite(sdaPin, LOW);
    delayMicroseconds(5);
    digitalWrite(sclPin, HIGH);
    delayMicroseconds(5);
    digitalWrite(sdaPin, HIGH);
    delayMicroseconds(10);

    return true;
}

// ── NVS Pin Configuration ─────────────────────────────────────────────────────
int SensorSentinel_i2c_get_sda_pin() {
    _i2cPrefs.begin("ss_i2c", true);
    int sda = _i2cPrefs.getInt("sda", _rtc_active_sda);
    _i2cPrefs.end();
    return sda;
}

int SensorSentinel_i2c_get_scl_pin() {
    _i2cPrefs.begin("ss_i2c", true);
    int scl = _i2cPrefs.getInt("scl", _rtc_active_scl);
    _i2cPrefs.end();
    return scl;
}

void SensorSentinel_i2c_set_pins(int sda, int scl) {
    _i2cPrefs.begin("ss_i2c", false);
    _i2cPrefs.putInt("sda", sda);
    _i2cPrefs.putInt("scl", scl);
    _i2cPrefs.end();
    _rtc_active_sda = sda;
    _rtc_active_scl = scl;
    _rtc_discovery_valid = false; // Force re-scan on new pins
}

// ── Low-level I2C helpers with timeout protection ─────────────────────────────
static bool i2c_ping(uint8_t address) {
    Wire.beginTransmission(address);
    return (Wire.endTransmission() == 0);
}

static bool i2c_read_16(uint8_t address, uint8_t reg, uint16_t* value) {
    Wire.beginTransmission(address);
    Wire.write(reg);
    if (Wire.endTransmission() != 0) return false;

    delayMicroseconds(50);

    if (Wire.requestFrom((uint8_t)address, (uint8_t)2) != 2) return false;
    uint8_t msb = Wire.read();
    uint8_t lsb = Wire.read();
    *value = ((uint16_t)msb << 8) | lsb;
    return true;
}

static bool i2c_write_16(uint8_t address, uint8_t reg, uint16_t value) {
    Wire.beginTransmission(address);
    Wire.write(reg);
    Wire.write((uint8_t)(value >> 8));
    Wire.write((uint8_t)(value & 0xFF));
    return (Wire.endTransmission() == 0);
}

// ── Sensor Driver Implementations ─────────────────────────────────────────────

// DFRobot SEN0291 / TI INA219 (Voltage mV, Current mA, Power mW)
static bool init_ina219(uint8_t address) {
    // Write Configuration Register 0x00: 32V FSR, 320mV shunt, 12-bit (0x399F)
    bool ok = i2c_write_16(address, 0x00, 0x399F);
    // Write Calibration Register 0x05 (for 0.1 ohm shunt, 2A range: cal = 4096)
    i2c_write_16(address, 0x05, 4096);
    return ok;
}

static bool read_ina219(uint8_t address, uint16_t* busVoltageMv, int16_t* currentMa, uint16_t* powerMw) {
    init_ina219(address);
    delay(10);

    uint16_t rawBus = 0;
    if (!i2c_read_16(address, 0x02, &rawBus)) {
        delay(10);
        if (!i2c_read_16(address, 0x02, &rawBus)) {
            Serial.printf("[I2C DEBUG] 0x%02X failed to read Reg 0x02\n", address);
            return false;
        }
    }

    // Bits 15:3 are bus voltage in 4mV steps
    *busVoltageMv = (rawBus >> 3) * 4;

    uint16_t rawShunt = 0;
    if (i2c_read_16(address, 0x01, &rawShunt)) {
        *currentMa = (int16_t)((int16_t)rawShunt / 10);
    } else {
        *currentMa = 0;
    }

    uint16_t rawPower = 0;
    if (i2c_read_16(address, 0x03, &rawPower)) {
        *powerMw = rawPower * 20;
    } else {
        *powerMw = ((uint32_t)(*busVoltageMv) * (uint32_t)abs(*currentMa)) / 1000;
    }
    return true;
}

// TI INA226
static bool init_ina226(uint8_t address) {
    // Config: Average 16, Vbus 1.1ms, Vshunt 1.1ms, Continuous
    return i2c_write_16(address, 0x00, 0x4527);
}

static bool read_ina226(uint8_t address, uint16_t* busVoltageMv, int16_t* currentMa) {
    uint16_t rawBus = 0;
    if (!i2c_read_16(address, 0x02, &rawBus)) return false;
    // INA226 Bus voltage: 1.25 mV / LSB
    *busVoltageMv = (uint16_t)((float)rawBus * 1.25f);

    uint16_t rawShunt = 0;
    if (i2c_read_16(address, 0x01, &rawShunt)) {
        *currentMa = (int16_t)rawShunt;
    } else {
        *currentMa = 0;
    }
    return true;
}

// ADS1115 4-Channel ADC (read single-ended AIN0..AIN3 in mV)
static bool read_ads1115(uint8_t address, uint8_t channel, uint16_t* millivolts) {
    if (channel > 3) return false;
    // Config: Single-shot, FSR ±4.096V (0x0200), 128SPS
    uint16_t config = 0x8183 | ((0x04 | channel) << 12);
    if (!i2c_write_16(address, 0x01, config)) return false;

    delay(9); // 128 SPS conversion time ~8ms

    uint16_t raw = 0;
    if (!i2c_read_16(address, 0x00, &raw)) return false;
    int16_t signedVal = (int16_t)raw;
    if (signedVal < 0) signedVal = 0;
    // FSR 4.096V: 0.125 mV per LSB
    *millivolts = (uint16_t)((float)signedVal * 0.125f);
    return true;
}

// BME280 / BMP280 (Temp in 0.1C, Humidity in 0.1%, Pressure in hPa)
static bool read_bme280(uint8_t address, int16_t* tempC10, uint16_t* hum10, uint16_t* pressureHpa) {
    Wire.beginTransmission(address);
    Wire.write(0xF7); // Press MSB
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)address, 6) != 6) return false;

    uint8_t data[6];
    for (int i = 0; i < 6; i++) data[i] = Wire.read();

    uint32_t rawPress = ((uint32_t)data[0] << 12) | ((uint32_t)data[1] << 4) | (data[2] >> 4);
    uint32_t rawTemp  = ((uint32_t)data[3] << 12) | ((uint32_t)data[4] << 4) | (data[5] >> 4);

    // Simplified calibrated approx for standard temps
    *tempC10 = (int16_t)((float)rawTemp / 5120.0f * 10.0f);
    *pressureHpa = (uint16_t)(rawPress / 256);
    *hum10 = 500; // 50.0% default if uncalibrated
    return true;
}

// BH1750 Light Sensor (Lux)
static bool read_bh1750(uint8_t address, uint16_t* lux) {
    Wire.beginTransmission(address);
    Wire.write(0x10); // Continuously H-Resolution Mode
    if (Wire.endTransmission() != 0) return false;
    delay(20);
    if (Wire.requestFrom((int)address, 2) != 2) return false;
    uint16_t raw = ((uint16_t)Wire.read() << 8) | Wire.read();
    *lux = (uint16_t)((float)raw / 1.2f);
    return true;
}

// ── Identify Sensor Type from I2C Address / Signature ─────────────────────────
static i2c_sensor_type_t identify_sensor(uint8_t addr) {
    // Check for SEN0291 / INA219 (default addresses 0x40, 0x41, 0x44, 0x45)
    if (addr == 0x40 || addr == 0x41 || addr == 0x44 || addr == 0x45) {
        uint16_t id = 0;
        if (i2c_read_16(addr, 0x00, &id)) {
            // INA219 power-on config default is 0x399F
            if (id == 0x399F || id == 0x319F || init_ina219(addr)) {
                return I2C_SENSOR_INA219;
            }
        }
    }

    // Check for INA226 (0x40 - 0x4F)
    if (addr >= 0x40 && addr <= 0x4F) {
        uint16_t dieId = 0;
        if (i2c_read_16(addr, 0xFF, &dieId) && (dieId == 0x2260 || dieId == 0x2261)) {
            init_ina226(addr);
            return I2C_SENSOR_INA226;
        }
    }

    // Check for ADS1115 (0x48 - 0x4B)
    if (addr >= 0x48 && addr <= 0x4B) {
        uint16_t cfg = 0;
        if (i2c_read_16(addr, 0x01, &cfg)) {
            return I2C_SENSOR_ADS1115;
        }
    }

    // Check for BME280 / BMP280 (0x76, 0x77)
    if (addr == 0x76 || addr == 0x77) {
        Wire.beginTransmission(addr);
        Wire.write(0xD0); // Chip ID register
        if (Wire.endTransmission(false) == 0 && Wire.requestFrom((int)addr, 1) == 1) {
            uint8_t chipId = Wire.read();
            if (chipId == 0x60) return I2C_SENSOR_BME280;
            if (chipId == 0x58) return I2C_SENSOR_BMP280;
        }
    }

    // Check for AHT20 / DHT20 (0x38)
    if (addr == 0x38) {
        return I2C_SENSOR_AHT20;
    }

    // Check for BH1750 (0x23)
    if (addr == 0x23) {
        return I2C_SENSOR_BH1750;
    }

    return I2C_SENSOR_GENERIC;
}

// ── Auto-Discovery Engine ─────────────────────────────────────────────────────
static bool test_i2c_bus(int sda, int scl) {
    SensorSentinel_i2c_bus_recovery(sda, scl);
    Wire.end();
    delay(5);
    if (!Wire.begin(sda, scl, 100000)) return false;
    Wire.setTimeOut(SENSORSENTINEL_I2C_READ_TIMEOUT_MS);

    // Quick sweep: check if at least one device ACK's
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        if (i2c_ping(addr)) {
            return true; // Found active device on this pin pair!
        }
    }
    return false;
}

uint8_t SensorSentinel_i2c_auto_discover(bool forceScan) {
    SensorSentinel_i2c_power_rail(true);

    if (_rtc_discovery_valid && !forceScan && _rtc_device_count > 0) {
        // Fast path for deep sleep wake: reuse cached pin mapping
        pinMode(37, INPUT); // Disconnect battery measurement divider from GPIO 1
        SensorSentinel_i2c_bus_recovery(_rtc_active_sda, _rtc_active_scl);
        Wire.end();
        Wire.begin(_rtc_active_sda, _rtc_active_scl, 100000);
        Wire.setTimeOut(100);
        _wireInitialized = true;
        return _rtc_device_count;
    }

    Serial.println("\n[I2C] Starting sensor auto-discovery...");

    // Candidate pin pairs to evaluate in priority order:
    // 1. User's configured pins (default: SDA=1, SCL=2)
    // 2. Flipped pins (SDA=2, SCL=1)
    // 3. Header pins (SDA=41, SCL=42 / SDA=33, SCL=34)
    // 4. OLED shared bus (SDA=17, SCL=18)
    struct PinPair { int sda; int scl; const char* desc; };
    PinPair candidatePairs[] = {
        { SensorSentinel_i2c_get_sda_pin(), SensorSentinel_i2c_get_scl_pin(), "Configured Pins" },
        { 2, 1, "Flipped Pins (SDA=2, SCL=1)" },
        { 41, 42, "Header J3 (SDA=41, SCL=42)" },
        { 33, 34, "Header J2 (SDA=33, SCL=34)" },
        { 17, 18, "OLED Bus (SDA=17, SCL=18)" }
    };

    int bestSda = -1;
    int bestScl = -1;

    for (size_t i = 0; i < sizeof(candidatePairs)/sizeof(candidatePairs[0]); i++) {
        Serial.printf("[I2C] Testing %s (SDA=%d, SCL=%d)...\n",
                      candidatePairs[i].desc, candidatePairs[i].sda, candidatePairs[i].scl);
        if (test_i2c_bus(candidatePairs[i].sda, candidatePairs[i].scl)) {
            bestSda = candidatePairs[i].sda;
            bestScl = candidatePairs[i].scl;
            Serial.printf("[I2C] Active I2C bus locked on SDA=%d, SCL=%d\n", bestSda, bestScl);
            break;
        }
    }

    if (bestSda < 0) {
        Serial.println("[I2C] No active I2C sensors detected on any pin candidates.");
        _rtc_device_count = 0;
        _rtc_discovery_valid = true;
        return 0;
    }

    _rtc_active_sda = bestSda;
    _rtc_active_scl = bestScl;
    _rtc_device_count = 0;
    uint8_t currentSlot = 0;

    // Full discovery scan on the locked bus
    for (uint8_t addr = 0x08; addr <= 0x77 && _rtc_device_count < SENSORSENTINEL_MAX_I2C_DEVICES; addr++) {
        if (!i2c_ping(addr)) continue;

        // Skip internal OLED address if on OLED bus
        if ((bestSda == 17 && bestScl == 18) && (addr == 0x3C || addr == 0x3D)) {
            continue;
        }

        i2c_sensor_type_t type = identify_sensor(addr);
        i2c_device_desc_t dev;
        memset(&dev, 0, sizeof(dev));
        dev.address = addr;
        dev.type = type;
        dev.active = true;
        dev.startSlot = currentSlot;

        switch (type) {
            case I2C_SENSOR_INA219:
                snprintf(dev.name, sizeof(dev.name), "SEN0291-0x%02X", addr);
                dev.numSlots = 3; // Slot 0: Volts mV, Slot 1: Curr mA, Slot 2: Power mW
                init_ina219(addr);
                break;
            case I2C_SENSOR_INA226:
                snprintf(dev.name, sizeof(dev.name), "INA226-0x%02X", addr);
                dev.numSlots = 2; // Slot 0: Volts mV, Slot 1: Curr mA
                init_ina226(addr);
                break;
            case I2C_SENSOR_ADS1115:
                snprintf(dev.name, sizeof(dev.name), "ADS1115-0x%02X", addr);
                dev.numSlots = 4; // 4 analog channels
                break;
            case I2C_SENSOR_BME280:
                snprintf(dev.name, sizeof(dev.name), "BME280-0x%02X", addr);
                dev.numSlots = 3; // Temp, Hum, Press
                break;
            case I2C_SENSOR_BMP280:
                snprintf(dev.name, sizeof(dev.name), "BMP280-0x%02X", addr);
                dev.numSlots = 2; // Temp, Press
                break;
            case I2C_SENSOR_BH1750:
                snprintf(dev.name, sizeof(dev.name), "BH1750-0x%02X", addr);
                dev.numSlots = 1; // Lux
                break;
            default:
                snprintf(dev.name, sizeof(dev.name), "I2C-0x%02X", addr);
                dev.numSlots = 1;
                break;
        }

        currentSlot += dev.numSlots;
        _rtc_devices[_rtc_device_count++] = dev;

        Serial.printf("[I2C] -> Discovered %s at address 0x%02X (Slots %d..%d)\n",
                      dev.name, dev.address, dev.startSlot, dev.startSlot + dev.numSlots - 1);
    }

    _rtc_discovery_valid = true;
    _wireInitialized = true;
    return _rtc_device_count;
}

// ── Read All Discovered Sensors Into Telemetry Slots ─────────────────────────
void SensorSentinel_i2c_read_all(uint16_t* sensorValues, uint8_t maxSlots) {
    if (!sensorValues || maxSlots == 0) return;
    memset(sensorValues, 0, maxSlots * sizeof(uint16_t));

    // Ensure power and bus are up
    SensorSentinel_i2c_power_rail(true);
    if (!_wireInitialized) {
        SensorSentinel_i2c_auto_discover(false);
    }

    for (uint8_t i = 0; i < _rtc_device_count; i++) {
        i2c_device_desc_t* dev = &_rtc_devices[i];
        if (!dev->active || dev->startSlot >= maxSlots) continue;

        switch (dev->type) {
            case I2C_SENSOR_INA219: {
                uint16_t vMv = 0, pMw = 0;
                int16_t cMa = 0;
                if (read_ina219(dev->address, &vMv, &cMa, &pMw)) {
                    if (dev->startSlot < maxSlots) sensorValues[dev->startSlot] = vMv;
                    if (dev->startSlot + 1 < maxSlots) sensorValues[dev->startSlot + 1] = (uint16_t)cMa;
                    if (dev->startSlot + 2 < maxSlots) sensorValues[dev->startSlot + 2] = pMw;
                    Serial.printf("[I2C] %s: %.2fV | %dmA | %dmW\n", dev->name, (float)vMv / 1000.0f, cMa, pMw);
                } else {
                    Serial.printf("[I2C] %s: Read timeout / error\n", dev->name);
                }
                break;
            }
            case I2C_SENSOR_INA226: {
                uint16_t vMv = 0;
                int16_t cMa = 0;
                if (read_ina226(dev->address, &vMv, &cMa)) {
                    if (dev->startSlot < maxSlots) sensorValues[dev->startSlot] = vMv;
                    if (dev->startSlot + 1 < maxSlots) sensorValues[dev->startSlot + 1] = (uint16_t)cMa;
                    Serial.printf("[I2C] %s: %.2fV | %dmA\n", dev->name, (float)vMv / 1000.0f, cMa);
                }
                break;
            }
            case I2C_SENSOR_ADS1115: {
                for (uint8_t ch = 0; ch < 4 && (dev->startSlot + ch) < maxSlots; ch++) {
                    uint16_t mv = 0;
                    if (read_ads1115(dev->address, ch, &mv)) {
                        sensorValues[dev->startSlot + ch] = mv;
                    }
                }
                break;
            }
            case I2C_SENSOR_BME280:
            case I2C_SENSOR_BMP280: {
                int16_t tempC10 = 0;
                uint16_t hum10 = 0, pressHpa = 0;
                if (read_bme280(dev->address, &tempC10, &hum10, &pressHpa)) {
                    if (dev->startSlot < maxSlots) sensorValues[dev->startSlot] = (uint16_t)tempC10;
                    if (dev->startSlot + 1 < maxSlots) sensorValues[dev->startSlot + 1] = hum10;
                    if (dev->startSlot + 2 < maxSlots) sensorValues[dev->startSlot + 2] = pressHpa;
                }
                break;
            }
            case I2C_SENSOR_BH1750: {
                uint16_t lux = 0;
                if (read_bh1750(dev->address, &lux)) {
                    sensorValues[dev->startSlot] = lux;
                }
                break;
            }
            default:
                break;
        }
    }
}

// ── Diagnostics Information ───────────────────────────────────────────────────
void SensorSentinel_i2c_init() {
    SensorSentinel_i2c_auto_discover(false);
}

void SensorSentinel_i2c_print_discovered() {
    Serial.println("\n--- I2C Discovered Devices ---");
    Serial.printf("Active Bus: SDA=%d, SCL=%d | Total Devices: %d\n",
                  _rtc_active_sda, _rtc_active_scl, _rtc_device_count);
    for (uint8_t i = 0; i < _rtc_device_count; i++) {
        Serial.printf(" [%d] 0x%02X: %-15s (Slots %d..%d)\n",
                      i + 1, _rtc_devices[i].address, _rtc_devices[i].name,
                      _rtc_devices[i].startSlot,
                      _rtc_devices[i].startSlot + _rtc_devices[i].numSlots - 1);
    }
    Serial.println("------------------------------");
}

uint8_t SensorSentinel_i2c_get_device_count() {
    return _rtc_device_count;
}

const i2c_device_desc_t* SensorSentinel_i2c_get_device(uint8_t index) {
    if (index >= _rtc_device_count) return NULL;
    return &_rtc_devices[index];
}

const char* SensorSentinel_i2c_type_to_string(i2c_sensor_type_t type) {
    switch (type) {
        case I2C_SENSOR_INA219: return "SEN0291/INA219";
        case I2C_SENSOR_INA226: return "INA226";
        case I2C_SENSOR_ADS1115: return "ADS1115";
        case I2C_SENSOR_BME280: return "BME280";
        case I2C_SENSOR_BMP280: return "BMP280";
        case I2C_SENSOR_AHT20: return "AHT20";
        case I2C_SENSOR_BH1750: return "BH1750";
        default: return "Generic";
    }
}
