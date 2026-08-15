# SensorSentinel Protocol Specification
**Version:** 1.0  
**Status:** Standard / Published  
**Author:** SensorSentinel Project  
**Transport:** LoRa (SX1262 / AU915 / US915 / EU868), BLE, UDP, MQTT  

---

## 1. Overview & Philosophy

The **SensorSentinel Protocol (v1.0)** is an ultra-low-power, open-source binary telemetry standard designed for marine, off-grid, and industrial remote monitoring.

### Key Characteristics:
* **Fixed 48-Byte Framing**: Compact structure optimized for LoRa radios ($\approx 82\text{ ms}$ Time-on-Air at SF7/125kHz), well below global duty cycle regulations.
* **Deterministic Little-Endian 4-Byte Alignment**: Packed binary layout (`__attribute__((packed))`) with zero compiler padding differences across 8-bit, 32-bit, and 64-bit architectures.
* **3-Tier Sensor Architecture**:
  1. **Tier 1: 16-Bit Digital Bitmask** (Physical switches, float switches, bilge runs, doors, alarms).
  2. **Tier 2: 4 Physical Analog ADC Channels** (Resistive tank senders for fuel/water/waste, 0–5V transducers).
  3. **Tier 3: 8 Modular Digital Bus Slots** (I2C/1-Wire/RS485 transducers, multi-battery wattmeters, environmental sensors).
* **Self-Discovery & Health**: Auto-reporting connected transducer counts, node battery percentage ($0-100\%$), and supply voltage.

---

## 2. Binary Packet Layout (48 Bytes)

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Protocol Ver  | Message Type  | Node Flags    |   Reserved    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                            Node ID                            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        Message Counter                        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         Uptime (sec)                          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Node Bat %    |       Node Supply Voltage (mV)        |  Pad  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    GPIO Digital Bitmask (16-bit)              |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|       GPIO Analog 0 (mV/Raw)  |       GPIO Analog 1 (mV/Raw)  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|       GPIO Analog 2 (mV/Raw)  |       GPIO Analog 3 (mV/Raw)  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|       Bus Slot 0 (mV/Val)     |       Bus Slot 1 (mA/Val)     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|       Bus Slot 2 (mW/Val)     |       Bus Slot 3 (mV/Val)     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|       Bus Slot 4 (mA/Val)     |       Bus Slot 5 (mW/Val)     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|       Bus Slot 6 (Val)        |       Bus Slot 7 (Val)        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Discovered Dev|                    Reserved                   |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

### Detailed Byte Field Map

| Byte Offset | Size | Field Name | Data Type | Description |
| :--- | :--- | :--- | :--- | :--- |
| `0` | **1 B** | `protocolVersion` | `uint8_t` | Protocol major version (`0x01`). |
| `1` | **1 B** | `messageType` | `uint8_t` | `0x01` = Sensor Telemetry, `0x02` = GNSS Location, `0x03` = Alarm, `0x04` = Heartbeat. |
| `2` | **1 B** | `nodeFlags` | `uint8_t` | Bit 0: Battery (`0`), Bit 1: Solar (`1`), Bit 7: Repeater Node (`0x80`). |
| `3` | **1 B** | `reserved0` | `uint8_t` | Alignment padding (`0x00`). |
| `4–7` | **4 B** | `nodeId` | `uint32_t` LE | Globally unique 32-bit Node ID derived from MAC/EUI. |
| `8–11` | **4 B** | `sequenceNumber` | `uint32_t` LE | Monotonically incrementing counter (persisted across RTC deep sleep). |
| `12–15` | **4 B** | `uptime` | `uint32_t` LE | Node uptime in seconds. |
| `16` | **1 B** | `batteryLevel` | `uint8_t` | Node internal 18650 charge ($0-100\%$). |
| `17–18` | **2 B** | `batteryVoltage` | `uint16_t` LE | Node internal supply rail in millivolts. |
| `19` | **1 B** | `reserved1` | `uint8_t` | Alignment padding (`0x00`). |
| `20–21` | **2 B** | `digitalBitmask` | `uint16_t` LE | 16 discrete binary inputs (D0–D15). |
| `22–29` | **8 B** | `gpioAnalog[4]` | `uint16_t[4]` LE | 4 physical ESP32 ADC channels (A0–A3) for tank level senders. |
| `30–45` | **16 B** | `busTelemetry[8]` | `uint16_t[8]` LE | 8 modular digital bus slots (Wattmeter V/I/P, Temperatures, etc.). |
| `46` | **1 B** | `discoveredSensors` | `uint8_t` | Number of auto-detected I2C transducers ($0-8$). |
| `47` | **1 B** | `reserved2` | `uint8_t` | Alignment padding (`0x00`). |

---

## 3. C/C++ Header Definition

```c
#pragma once
#include <stdint.h>

#define SENSORSENTINEL_PROTOCOL_VERSION 0x01

#define SS_MSG_SENSOR      0x01 // Strategic Sensor Telemetry (48 Bytes)
#define SS_MSG_GNSS        0x02 // GNSS Navigation & Location (35 Bytes)
#define SS_MSG_ALARM       0x03 // State Transition Alarm
#define SS_MSG_HEARTBEAT   0x04 // System Diagnostic Keepalive

typedef struct __attribute__((packed)) {
    // Common Header (18 Bytes)
    uint8_t  protocolVersion;     // 0x01
    uint8_t  messageType;         // 0x01
    uint8_t  nodeFlags;           // Power source & node mode
    uint8_t  reserved0;           // Alignment
    uint32_t nodeId;              // 32-bit Node ID
    uint32_t messageCounter;      // Sequence number
    uint32_t uptime;              // Uptime in seconds
    uint8_t  batteryLevel;        // Node 18650 Battery % (0-100)
    uint16_t batteryVoltage;      // Node 18650 Voltage (mV)
    uint8_t  reserved1;

    // Physical GPIO Tier (10 Bytes)
    uint16_t digitalBitmask;      // 16 Binary Inputs (D0..D15)
    uint16_t gpioAnalog[4];       // 4 Physical ADC Channels (A0..A3)

    // Digital Bus Tier (18 Bytes)
    uint16_t busTelemetry[8];     // 8 Modular Bus Slots (Wattmeters, Temp/Hum)
    uint8_t  discoveredSensors;   // Count of auto-detected I2C devices
    uint8_t  reserved2;
} SensorSentinel_sensor_packet_t;
```

---

## 4. Standard Gateway JSON Schema

When received by gateways, the 48-byte binary frame unpacks into standard JSON:

```json
{
  "protocol": 1,
  "type": "sensor",
  "nodeId": 2697689064,
  "nodeFlags": 0,
  "counter": 84,
  "uptime": 0,
  "battery": 100,
  "voltage": 4150,
  "gpio": {
    "digitalBitmask": 255,
    "digital": [1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0],
    "analog": [0, 0, 3123, 60]
  },
  "bus": {
    "discoveredSensors": 1,
    "slots": [12784, 0, 40, 0, 0, 0, 0, 0]
  }
}
```

---

## 5. PostgreSQL Schema Mapping

The database mirrors the 3 tiers with dedicated relational tables:

```sql
-- 1. Digital Binary Inputs (16 Pins)
CREATE TABLE digital_pins (
    id SERIAL PRIMARY KEY,
    device_id INTEGER NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
    pin_index SMALLINT NOT NULL CHECK (pin_index BETWEEN 0 AND 15),
    label VARCHAR(100) DEFAULT '',
    trigger VARCHAR(10) DEFAULT 'None' CHECK (trigger IN ('None', 'High', 'Low', 'Change')),
    alert_level VARCHAR(10) DEFAULT 'None' CHECK (alert_level IN ('None', 'Low', 'Medium', 'High')),
    UNIQUE(device_id, pin_index)
);

-- 2. Physical GPIO Analog ADCs (4 Channels)
CREATE TABLE analog_pins (
    id SERIAL PRIMARY KEY,
    device_id INTEGER NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
    pin_index SMALLINT NOT NULL CHECK (pin_index BETWEEN 0 AND 3),
    label VARCHAR(100) DEFAULT '',
    low_threshold NUMERIC,
    high_threshold NUMERIC,
    alert_level VARCHAR(10) DEFAULT 'None' CHECK (alert_level IN ('None', 'Low', 'Medium', 'High')),
    UNIQUE(device_id, pin_index)
);

-- 3. Digital Bus Telemetry Slots (8 Slots for I2C / Wattmeters)
CREATE TABLE bus_pins (
    id SERIAL PRIMARY KEY,
    device_id INTEGER NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
    pin_index SMALLINT NOT NULL CHECK (pin_index BETWEEN 0 AND 7),
    label VARCHAR(100) DEFAULT '',
    low_threshold NUMERIC,
    high_threshold NUMERIC,
    alert_level VARCHAR(10) DEFAULT 'None' CHECK (alert_level IN ('None', 'Low', 'Medium', 'High')),
    UNIQUE(device_id, pin_index)
);
```
