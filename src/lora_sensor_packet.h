// Available pins for external use (analog and digital)
#if defined(ARDUINO_heltec_wireless_tracker)
  // Wireless Tracker available pins
  #define HELTEC_ANALOG_PINS     {4, 5, 6, 7}
  #define HELTEC_BOOLEAN_PINS    {15, 16, 17, 18, 43, 44, 45, 46}
  #define HELTEC_ANALOG_COUNT    4
  #define HELTEC_BOOLEAN_COUNT   8
  #define HELTEC_J2_HEADER       // Has J2 header
  #define HELTEC_J3_HEADER       // Has J3 header

#elif defined(ARDUINO_heltec_wifi_32_lora_V3) || defined(BOARD_HELTEC_V3_2)
  // WiFi LoRa 32 V3/V3.2 available pins
  #define HELTEC_ANALOG_PINS     {1, 2, 3, 4}
  #define HELTEC_BOOLEAN_PINS    {33, 34, 35, 39, 40, 41, 42, 46}
  #define HELTEC_ANALOG_COUNT    4
  #define HELTEC_BOOLEAN_COUNT   8

#elif defined(ARDUINO_heltec_wireless_stick)
  // Wireless Stick available pins
  #define HELTEC_ANALOG_PINS     {1, 2, 3, 6}
  #define HELTEC_BOOLEAN_PINS    {7, 8, 9, 10, 11, 12, 13, 15}
  #define HELTEC_ANALOG_COUNT    4
  #define HELTEC_BOOLEAN_COUNT   8

#elif defined(ARDUINO_heltec_wireless_stick_lite)
  // Wireless Stick Lite available pins
  #define HELTEC_ANALOG_PINS     {1, 2, 3, 6}
  #define HELTEC_BOOLEAN_PINS    {7, 8, 9, 10, 11, 12, 13, 15}
  #define HELTEC_ANALOG_COUNT    4
  #define HELTEC_BOOLEAN_COUNT   8

#else
  // Default pins (may not work on all boards)
  #define HELTEC_ANALOG_PINS     {1, 2, 3, 4}
  #define HELTEC_BOOLEAN_PINS    {33, 34, 35, 36, 37, 38, 39, 40}
  #define HELTEC_ANALOG_COUNT    4
  #define HELTEC_BOOLEAN_COUNT   8
#endif

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
 * @brief Prints the available pins for the current board
 */
void heltec_print_available_pins() {
  both.printf("Board: %s\n", heltec_get_board_name());
  both.printf("Available pins:\n");
  
  // Print analog pins
  both.printf("Analog (%d): ", HELTEC_ANALOG_COUNT);
  for (uint8_t i = 0; i < HELTEC_ANALOG_COUNT; i++) {
    both.printf("GPIO%d", heltec_analog_pins[i]);
    if (i < HELTEC_ANALOG_COUNT - 1) both.printf(", ");
  }
  both.println();
  
  // Print digital pins
  both.printf("Digital (%d): ", HELTEC_BOOLEAN_COUNT);
  for (uint8_t i = 0; i < HELTEC_BOOLEAN_COUNT; i++) {
    both.printf("GPIO%d", heltec_boolean_pins[i]);
    if (i < HELTEC_BOOLEAN_COUNT - 1) both.printf(", ");
  }
  both.println();

  #ifdef HELTEC_J2_HEADER
    both.println("J2 header available");
  #endif
  #ifdef HELTEC_J3_HEADER
    both.println("J3 header available");
  #endif
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
uint8_t heltec_read_boolean_byte(uint8_t maxBits = 8) {
  uint8_t result = 0;
  uint8_t count = maxBits < HELTEC_BOOLEAN_COUNT ? maxBits : HELTEC_BOOLEAN_COUNT; 
  for (uint8_t i = 0; i < count; i++) {
    result |= (heltec_read_boolean(i) & 0x01) << i;
  }
  
  return result;
}

/**
 * @brief Struct for holding pin readings for packet transmission
 */
typedef struct {
  uint16_t analog[4];   // 4 analog values
  uint8_t boolean;      // 8 boolean values packed as bits
} heltec_pin_readings_t;

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
 * @brief Standard status packet structure for LoRa transmission
 */
typedef struct {
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
} __attribute__((packed)) heltec_status_packet_t;

/**
 * @brief Populate a status packet with current board data
 * 
 * @param packet Pointer to packet structure to populate
 * @param messageCounter Message counter value to include
 * @param includeGPS Whether to include GPS data (if available)
 * @return Size of the packet in bytes
 */
size_t heltec_build_status_packet(heltec_status_packet_t* packet, uint32_t messageCounter, bool includeGPS = false) {
  // Get board ID from MAC address if not already set
  static uint32_t cachedBoardId = 0;
  if (cachedBoardId == 0) {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    cachedBoardId = (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5]; // Last 4 bytes
  }
  
  // Basic packet data
  packet->messageType = includeGPS ? 0x02 : 0x01;
  packet->boardId = cachedBoardId;
  packet->messageCounter = messageCounter;
  packet->batteryPercent = heltec_battery_percent();
  packet->uptime = millis();
  
  // Read pin data
  heltec_read_all_pins((heltec_pin_readings_t*)&(packet->analog));
  
  // Add GPS data if requested
  if (includeGPS) {
    #ifdef HELTEC_GNSS
      packet->latitude = gps.location.isValid() ? gps.location.lat() : 0.0f;
      packet->longitude = gps.location.isValid() ? gps.location.lng() : 0.0f;
      packet->altitude = gps.altitude.isValid() ? (int16_t)gps.altitude.meters() : 0;
      packet->satellites = gps.satellites.value();
    #else
      packet->latitude = 0.0f;
      packet->longitude = 0.0f;
      packet->altitude = 0;
      packet->satellites = 0;
    #endif
    return sizeof(heltec_status_packet_t);
  } else {
    // Return size without GPS data
    return sizeof(heltec_status_packet_t) - 12;
  }
}

/**
 * @brief Print packet information to Serial and display
 * 
 * @param packet Pointer to packet structure
 * @param includeGPS Whether GPS data should be displayed
 */
void heltec_print_packet_info(heltec_status_packet_t* packet, bool includeGPS = false) {
  // Serial output (detailed)
  Serial.println("--- Packet Info ---");
  Serial.printf("Board: %s\n", heltec_get_board_name());
  Serial.printf("Message Type: %s (0x%02X)\n", 
                packet->messageType == 0x02 ? "GPS" : "Status", 
                packet->messageType);
  Serial.printf("Board ID: %08X\n", packet->boardId);
  Serial.printf("Message #: %u\n", packet->messageCounter);
  
  for (size_t i = 0; i < 4; i++) {
    Serial.printf("Analog %d (GPIO%d): %u\n", i, 
                  i < heltec_get_analog_count() ? heltec_get_analog_pin(i) : -1, 
                  packet->analog[i]);
  }
  
  for (size_t i = 0; i < 8; i++) {
    Serial.printf("Boolean %d (GPIO%d): %d\n", i, 
                  i < heltec_get_boolean_count() ? heltec_get_boolean_pin(i) : -1,
                  (packet->boolean >> i) & 0x01);
  }
  
  Serial.printf("Battery: %u%%\n", packet->batteryPercent);
  Serial.printf("Uptime: %u s\n", packet->uptime / 1000);
  
  if (includeGPS) {
    Serial.println("GPS Data:");
    Serial.printf("  Latitude: %.6f\n", packet->latitude);
    Serial.printf("  Longitude: %.6f\n", packet->longitude);
    Serial.printf("  Altitude: %d m\n", packet->altitude);
    Serial.printf("  Satellites: %u\n", packet->satellites);
  }
  
  // Display output (concise)
  both.printf("Msg:%u ID:%08X\n", packet->messageCounter, packet->boardId);
  both.printf("Type:%s\n", packet->messageType == 0x02 ? "GPS" : "Status");
  
  // Optional display of analog/digital values based on display size
  #if defined(HELTEC_WIDE_DISPLAY)
    both.printf("A:%u,%u,%u,%u\n", 
               packet->analog[0], packet->analog[1], 
               packet->analog[2], packet->analog[3]);
    both.printf("D:%02X B:%u%% Up:%us\n", 
               packet->boolean, packet->batteryPercent, packet->uptime/1000);
  #endif
 
  if (includeGPS) {
    both.printf("Lat:%.6f\n", packet->latitude);
    both.printf("Lon:%.6f\n", packet->longitude);
    both.printf("Alt:%dm Sats:%u\n", packet->altitude, packet->satellites);
  }
}