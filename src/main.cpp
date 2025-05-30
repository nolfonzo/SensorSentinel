/**
 * Heltec Wireless Tracker - Bidirectional LoRa Transceiver
 * - Sends packets automatically every PAUSE seconds
 * - Can send immediately with button press
 * - Always listening for incoming packets
 * - Shows RSSI/SNR for received packets
 * - Includes 1% duty cycle compliance
 * - Built for Heltec Wireless Tracker board
 */

#define HELTEC_POWER_BUTTON     // Enable power button functionality
#define HELTEC_WIRELESS_TRACKER // Specify the board type
#include <heltec_unofficial.h>
#include <WiFi.h> // For MAC address

// Configuration
#define PAUSE               10          // Seconds between auto-transmissions (0 = manual only)
#define FREQUENCY           915.0       // MHz (use 868.0 for Europe)
#define BANDWIDTH           125.0       // kHz
#define SPREADING_FACTOR    9           // 6-12 (higher = longer range, slower)
#define TRANSMIT_POWER      14          // dBm (0-22, check regulations)

// Pin assignments for sensors (free pins from datasheet)
const uint8_t ANALOG_PINS[4] = {4, 5, 6, 7}; // J2, ADC1
const uint8_t BOOLEAN_PINS[8] = {15, 16, 17, 18, 43, 44, 45, 46}; // J3

// Packet structure
struct StatusPacket {
  uint8_t messageType;      // 0x01 (no GPS) or 0x02 (with GPS)
  uint32_t boardId;         // Last 4 bytes of MAC address
  uint32_t messageCounter;  // 32-bit counter
  uint16_t analog[4];       // 4 analog values (16-bit each)
  uint8_t boolean;          // 8 boolean values (1 bit each)
  uint8_t batteryPercent;   // 0-100%
  uint32_t uptime;          // Milliseconds since boot
  // Optional GPS data (if messageType = 0x02)
  float latitude;           // Degrees
  float longitude;          // Degrees
  int16_t altitude;         // Meters
  uint16_t satellites;      // Number of satellites
} __attribute__((packed));

// Global variables
String rxdata;
volatile bool rxFlag = false;
long messageCounter = 0;
uint64_t lastTxTime = 0;
uint64_t txDuration;
uint64_t minimumPause;
bool pendingGpsTransmit = false;
bool gpsInitialized = false;
uint32_t boardId; // Unique board ID

void rx() {
  rxFlag = true;
}

void initializeDisplay(uint8_t textSize = 1, uint8_t rotation = 1) {
  display.fillScreen(ST7735_BLACK);
  display.setTextColor(ST7735_WHITE);
  display.setCursor(0, 0);
  display.setRotation(rotation);
  display.setTextSize(textSize);
}

void initializeGPS() {
  initializeDisplay();
  if (!gpsInitialized) {
    both.println("Initializing GPS...");
    heltec_gnss_begin();
    gpsInitialized = true;
    both.println("GPS initialized");
    delay(1000);
  }
}

void transmitPacket(bool sendGPS = false) {
  StatusPacket packet;
  packet.messageType = sendGPS ? 0x02 : 0x01;
  packet.boardId = boardId;
  packet.messageCounter = messageCounter++;
  for (size_t i = 0; i < 4; i++) {
    packet.analog[i] = analogRead(ANALOG_PINS[i]);
  }
  packet.boolean = 0;
  for (size_t i = 0; i < 8; i++) {
    packet.boolean |= (digitalRead(BOOLEAN_PINS[i]) & 0x01) << i;
  }
  packet.batteryPercent = heltec_battery_percent();
  packet.uptime = millis();
  size_t packetSize = sizeof(StatusPacket) - (sendGPS ? 0 : 12);
  if (sendGPS) {
    packet.latitude = gps.location.isValid() ? gps.location.lat() : 0.0f;
    packet.longitude = gps.location.isValid() ? gps.location.lng() : 0.0f;
    packet.altitude = gps.altitude.isValid() ? (int16_t)gps.altitude.meters() : 0;
    packet.satellites = gps.satellites.value();
  }
  // Serial output (detailed)
  Serial.println("--- Packet Sent ---");
  Serial.printf("Message Type: %s (0x%02X)\n", sendGPS ? "GPS" : "Status", packet.messageType);
  Serial.printf("Board ID: %08X\n", packet.boardId);
  Serial.printf("Message #: %u\n", packet.messageCounter);
  for (size_t i = 0; i < 4; i++) {
    Serial.printf("Analog %d: %u\n", i, packet.analog[i]);
  }
  for (size_t i = 0; i < 8; i++) {
    Serial.printf("Boolean D%d: %d\n", i, (packet.boolean >> i) & 0x01);
  }
  Serial.printf("Battery: %u%%\n", packet.batteryPercent);
  Serial.printf("Uptime: %u s\n", packet.uptime / 1000);
  if (sendGPS) {
    Serial.println("GPS Data:");
    Serial.printf("  Latitude: %.6f\n", packet.latitude);
    Serial.printf("  Longitude: %.6f\n", packet.longitude);
    Serial.printf("  Altitude: %d m\n", packet.altitude);
    Serial.printf("  Satellites: %u\n", packet.satellites);
  }
  // TFT output (concise)
  display.printf("Msg:%u\n", packet.messageCounter);
  display.printf("GPS:%s\n", sendGPS ? "true" : "false");
  display.printf("Id:%08X\n", packet.boardId);
  radio.clearDio1Action();
  heltec_led(50);
  txDuration = millis();
  int state = radio.transmit((uint8_t*)&packet, packetSize);
  txDuration = millis() - txDuration;
  heltec_led(0);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.printf("Status: OK (%i ms)\n", (int)txDuration);
    display.printf("OK (%i ms)\n", (int)txDuration);
  } else {
    Serial.printf("Status: FAIL (%i)\n", state);
    display.printf("FAIL (%i)\n", state);
    Serial.printf("Transmit failed, state: %i\n", state); // Debug
  }
  Serial.println("------------------");
  minimumPause = txDuration * 100;
  lastTxTime = millis();
  radio.setDio1Action(rx);
  int rxState = radio.startReceive(RADIOLIB_SX126X_RX_TIMEOUT_INF);
  if (rxState != RADIOLIB_ERR_NONE) {
    Serial.printf("startReceive failed, state: %i\n", rxState); // Debug
  }
}
void handleReceivedPacket() {
  rxFlag = false;
  uint8_t buffer[sizeof(StatusPacket)];
  int state = radio.readData(buffer, sizeof(StatusPacket));
  if (state == RADIOLIB_ERR_NONE) {
    StatusPacket* packet = (StatusPacket*)buffer;
    size_t length = radio.getPacketLength();
    if (length >= 20) { // Minimum packet size
      // Serial output (detailed)
      Serial.println("--- Packet Received ---");
      Serial.printf("Message Type: %s (0x%02X)\n", packet->messageType == 0x02 ? "GPS" : "Status", packet->messageType);
      Serial.printf("Board ID: %08X\n", packet->boardId);
      Serial.printf("Message #: %u\n", packet->messageCounter);
      for (int i = 0; i < 4; i++) {
        Serial.printf("Analog %d: %u\n", i, packet->analog[i]);
      }
      for (int i = 0; i < 8; i++) {
        Serial.printf("Boolean D%d: %d\n", i, (packet->boolean >> i) & 0x01);
      }
      Serial.printf("Battery: %u%%\n", packet->batteryPercent);
      Serial.printf("Uptime: %u s\n", packet->uptime / 1000);
      if (packet->messageType == 0x02 && length >= 32) {
        Serial.println("GPS Data:");
        Serial.printf("  Latitude: %.6f\n", packet->latitude);
        Serial.printf("  Longitude: %.6f\n", packet->longitude);
        Serial.printf("  Altitude: %d m\n", packet->altitude);
        Serial.printf("  Satellites: %u\n", packet->satellites);
      }
      Serial.printf("RSSI: %.1f dBm\n", radio.getRSSI());
      Serial.printf("SNR: %.1f dB\n", radio.getSNR());
      Serial.println("------------------");
      // TFT output (concise)
      initializeDisplay(2);
      display.printf("RX Board:%08X\nMsg#%u\n", packet->boardId, packet->messageCounter);
      display.printf("A0:%u A1:%u\nA2:%u A3:%u\n", packet->analog[0], packet->analog[1], packet->analog[2], packet->analog[3]);
      display.printf("D0-7:%02X Bat:%u%%\n", packet->boolean, packet->batteryPercent);
      display.printf("Up:%us\n", packet->uptime / 1000);
      if (packet->messageType == 0x02 && length >= 32) {
        display.printf("Lat:%.6f\nLon:%.6f\n", packet->latitude, packet->longitude);
        display.printf("Alt:%dm Sats:%u\n", packet->altitude, packet->satellites);
      }
      display.printf("RSSI:%.1f SNR:%.1f\n", radio.getRSSI(), radio.getSNR());
      display.println("---");
      heltec_led(25);
      delay(100);
      heltec_led(0);
    } else {
      // Serial error
      Serial.println("--- Packet Error ---");
      Serial.printf("Invalid length: %u\n", length);
      Serial.println("------------------");
      // TFT error
      initializeDisplay(2);
      display.printf("RX Error: Length %u\n", length);
    }
  } else {
    // Serial error
    Serial.println("--- Packet Error ---");
    Serial.printf("State: %i\n", state);
    Serial.println("------------------");
    // TFT error
    initializeDisplay(2);
    display.printf("RX Error: %i\n", state);
  }
  RADIOLIB_OR_HALT(radio.startReceive(RADIOLIB_SX126X_RX_TIMEOUT_INF));
}

void setup() {
  heltec_setup();
  initializeDisplay(1);
  both.println("Heltec Tracker. Init...");
  // Get board ID from MAC address
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  boardId = (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5]; // Last 4 bytes
  RADIOLIB_OR_HALT(radio.begin());
  radio.setDio1Action(rx);
  both.printf("Frequency: %.1f MHz\n", FREQUENCY);
  RADIOLIB_OR_HALT(radio.setFrequency(FREQUENCY));
  both.printf("Bandwidth: %.1f kHz\n", BANDWIDTH);
  RADIOLIB_OR_HALT(radio.setBandwidth(BANDWIDTH));
  both.printf("Spreading Factor: %i\n", SPREADING_FACTOR);
  RADIOLIB_OR_HALT(radio.setSpreadingFactor(SPREADING_FACTOR));
  both.printf("TX Power: %i dBm\n", TRANSMIT_POWER);
  RADIOLIB_OR_HALT(radio.setOutputPower(TRANSMIT_POWER));
  both.println("Starting to listen...");
  RADIOLIB_OR_HALT(radio.startReceive(RADIOLIB_SX126X_RX_TIMEOUT_INF));
  if (PAUSE > 0) {
    both.printf("Auto-tx every %i sec\n", PAUSE);
  } else {
    both.println("Manual tx (press button)");
  }
  both.println("Button = GPS location");
  both.println("Wireless Tracker ready!");
  both.println("---");
  // Initialize pins
  for (int i = 0; i < 8; i++) {
    pinMode(BOOLEAN_PINS[i], INPUT);
  }
}

void loop() {
  heltec_loop();
  button.update();
  bool txLegal = millis() > lastTxTime + minimumPause;
  bool timeToTransmit = (PAUSE > 0) && (millis() - lastTxTime > (PAUSE * 1000));
  bool buttonPressed = button.isSingleClick();
  if (buttonPressed) {
    initializeGPS();
    if (gpsInitialized) {
      heltec_gnss_update();
      int sats = gps.satellites.value();
      if (sats == 0) {
        initializeDisplay(2);
        both.println("GPS Status");
        both.println("No satellites");
        both.println("acquired");
        delay(2000);
        return;
      } else {
        pendingGpsTransmit = true;
        initializeDisplay(2);
        both.printf("GPS ready\n%i satellites\nQueued for TX", sats);
        delay(1000);
      }
    }
  }
  if (pendingGpsTransmit && txLegal) {
    if (gpsInitialized) {
      heltec_gnss_update();
    }
    initializeDisplay(2);
    transmitPacket(true);
    pendingGpsTransmit = false;
  } else if (timeToTransmit && txLegal) {
    initializeDisplay(2);
    transmitPacket(false);
  } else if (pendingGpsTransmit && !txLegal) {
    int waitSeconds = ((minimumPause - (millis() - lastTxTime)) / 1000) + 1;
    initializeDisplay(2);
    both.printf("GPS Tx queued\nDuty cycle\nWait %i secs", waitSeconds);
  }
  if (rxFlag) {
    handleReceivedPacket();
  }
}