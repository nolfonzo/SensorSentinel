/**  
 * @file heltec_unofficial.cpp  
 * @brief Implementation file for the Heltec ESP32 LoRa library.  
 */  

#include "heltec_unofficial.h"  

// ====== Global variable definitions ======  

// Flag to track if display needs updating (for V3.2)  
bool _display_needs_update = false;  

// HotButton instance for the PRG button  
HotButton button(BUTTON);  

#ifndef HELTEC_NO_RADIOLIB  
  // Create RadioLib instance based on board type  
  #if defined(ARDUINO_heltec_wireless_stick) || defined(ARDUINO_heltec_wireless_stick_lite)  
    // Stick and Stick Lite use SX1276  
    SX1276 radio = new Module(SS, DIO1, RST_LoRa, BUSY_LoRa);  
  #elif defined(ARDUINO_heltec_wireless_tracker)  
    // Tracker needs HSPI for LoRa (VSPI used for TFT)  
    SPIClass* hspi = new SPIClass(HSPI);  
    SX1262 radio = new Module(SS, DIO1, RST_LoRa, BUSY_LoRa, *hspi);  
  #else  
    // V3 and V3.2 use SX1262  
    SX1262 radio = new Module(SS, DIO1, RST_LoRa, BUSY_LoRa);  
  #endif  
#endif  

// Create Display instance based on board type  
#ifndef HELTEC_NO_DISPLAY  
  #ifdef ARDUINO_heltec_wireless_tracker  
    Adafruit_ST7735 display(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);  
    PrintSplitter both(Serial, display);  
  #elif defined(ARDUINO_heltec_wifi_32_lora_V3)  
    SSD1306Wire display(0x3c, SDA_OLED, SCL_OLED, DISPLAY_GEOMETRY);  
    PrintSplitter both(Serial, display);  
  #else  
    Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, RST_OLED);  
    PrintSplitter both(Serial, display);  
  #endif  
#else  
  // No display - just use Serial  
  Print &both = Serial;  
#endif  

#ifdef HELTEC_GNSS  
  // Global GNSS parser for NMEA-compatible GNSS modules  
  TinyGPSPlus gps;  
  // UART1 for GNSS communication  
  HardwareSerial gpsSerial = HardwareSerial(1);  
#endif  

// Battery voltage calibration  
const float min_voltage = 3.04;  
const float max_voltage = 4.26;  
const uint8_t scaled_voltage[100] = {  
  254, 242, 230, 227, 223, 219, 215, 213, 210, 207,  
  206, 202, 202, 200, 200, 199, 198, 198, 196, 196,  
  195, 195, 194, 192, 191, 188, 187, 185, 185, 185,  
  183, 182, 180, 179, 178, 175, 175, 174, 172, 171,  
  170, 169, 168, 166, 166, 165, 165, 164, 161, 161,  
  159, 158, 158, 157, 156, 155, 151, 148, 147, 145,  
  143, 142, 140, 140, 136, 132, 130, 130, 129, 126,  
  125, 124, 121, 120, 118, 116, 115, 114, 112, 112,  
  110, 110, 108, 106, 106, 104, 102, 101, 99, 97,  
  94, 90, 81, 80, 76, 73, 66, 52, 32, 7  
};  

// Button state tracking  
bool buttonClicked = false;  

// Global variables for packet subscription system  
PacketCallback _packetCallback = NULL;  
volatile bool _packetReceived = false;  
String _packetData;  
float _packetRssi;  
float _packetSnr;  

// ====== Implementation of PrintSplitter methods ======  

size_t PrintSplitter::write(uint8_t c) {  
  a.write(c);  
  size_t r = b.write(c);  
  #if defined(BOARD_HELTEC_V3_2)  
    _display_needs_update = true;  
  #endif  
  return r;  
}  

size_t PrintSplitter::write(const uint8_t *buffer, size_t size) {  
  a.write(buffer, size);  
  size_t r = b.write(buffer, size);  
  #if defined(BOARD_HELTEC_V3_2)  
    _display_needs_update = true;  
  #endif  
  return r;  
}  

// ====== Function implementations ======  

/**  
 * @brief Gets a string describing the current board  
 *   
 * @return Board description  
 */  
const char* heltec_get_board_name() {  
  #if defined(ARDUINO_heltec_wireless_tracker)  
    return "Wireless Tracker";  
  #elif defined(BOARD_HELTEC_V3_2)  
    return "WiFi LoRa 32 V3.2";  
  #elif defined(ARDUINO_heltec_wifi_32_lora_V3)  
    return "WiFi LoRa 32 V3";  
  #elif defined(ARDUINO_heltec_wireless_stick)  
    return "Wireless Stick";  
  #elif defined(ARDUINO_heltec_wireless_stick_lite)  
    return "Wireless Stick Lite";  
  #else  
    return "Unknown Heltec Board";  
  #endif  
}  

/**  
 * @brief Updates the display buffer to the screen.  
 *   
 * This is necessary for V3.2 boards using the Adafruit SSD1306 library.  
 */  
void heltec_display_update() {  
  #ifndef HELTEC_NO_DISPLAY  
    #if defined(BOARD_HELTEC_V3_2)  
      if (_display_needs_update) {  
        display.display();  
        _display_needs_update = false;  
      }  
    #endif  
  #endif  
}  

/**  
 * @brief Controls the LED brightness based on the given percentage.  
 *  
 * @param percent The brightness percentage of the LED (0-100).  
 */  
void heltec_led(int percent) {  
  if (percent > 0) {  
    ledcSetup(LED_CHAN, LED_FREQ, LED_RES);  
    ledcAttachPin(LED_PIN, LED_CHAN);  
    ledcWrite(LED_CHAN, percent * 255 / 100);  
  } else {  
    ledcDetachPin(LED_PIN);  
    pinMode(LED_PIN, INPUT);  
  }  
}  

/**  
 * @brief Controls the VEXT pin to enable or disable external power.  
 *  
 * @param state The state of the VEXT pin (true = enable, false = disable).  
 */  
void heltec_ve(bool state) {  
  if (state) {  
    pinMode(VEXT, OUTPUT);  
    digitalWrite(VEXT, LOW);  
    delay(100);  
  } else {  
    // pulled up, no need to drive it  
    pinMode(VEXT, INPUT);  
  }  
}  

/**  
 * @brief Controls the TFT power and backlight for Wireless Tracker.  
 *   
 * @param state True to enable, false to disable  
 */  
void heltec_tft_power(bool state) {  
  #ifdef ARDUINO_heltec_wireless_tracker  
    if (state) {  
      pinMode(TFT_VTFT, OUTPUT);  
      digitalWrite(TFT_VTFT, HIGH);  
      pinMode(TFT_LED, OUTPUT);  
      digitalWrite(TFT_LED, HIGH);  
    } else {  
      digitalWrite(TFT_VTFT, LOW);  
      digitalWrite(TFT_LED, LOW);  
      pinMode(TFT_VTFT, INPUT);  
      pinMode(TFT_LED, INPUT);  
    }  
  #endif  
}  

/**  
 * @brief Measures the battery voltage.  
 *  
 * @return The battery voltage in volts.  
 */  
float heltec_vbat() {  
  pinMode(VBAT_CTRL, OUTPUT);  
  digitalWrite(VBAT_CTRL, LOW);  
  delay(5);  
  float vbat;  
  #ifdef ARDUINO_heltec_wireless_tracker  
    vbat = analogRead(VBAT_ADC) * 4.9 / 4095.0; // 12-bit ADC, Vbat_Read*4.9  
  #else  
    vbat = analogRead(VBAT_ADC) / 238.7; // Original calibration  
  #endif  
  pinMode(VBAT_CTRL, INPUT);  
  return vbat;  
}  

#ifdef HELTEC_GNSS  
  /**  
   * @brief Initializes the GNSS module  
   */  
  void heltec_gnss_begin() {  
    heltec_ve(true);  // Power on GNSS via Vext  
    delay(100);  
    gpsSerial.begin(9600, SERIAL_8N1, GNSS_RX, GNSS_TX);  
  }  

  /**  
   * @brief Puts the GNSS module to sleep  
   */  
  void heltec_gnss_sleep() {  
    gpsSerial.end();  
    pinMode(GNSS_TX, INPUT);  
    pinMode(GNSS_RX, INPUT);  
    // Vext power managed by heltec_ve(false) elsewhere  
  }  

  /**  
   * @brief Updates GNSS data if available  
   *   
   * @return true if new data was processed  
   */  
  bool heltec_gnss_update() {  
    if (!gpsSerial) return false;  
    if (digitalRead(VEXT) == HIGH) return false; // Check VEXT state (LOW=on)  
    while (gpsSerial.available()) {  
      if (gps.encode(gpsSerial.read())) {  
        return true; // New data available  
      }  
    }  
    return false;  
  }  
#endif  

/**  
 * @brief Puts the device into deep sleep mode.  
 *  
 * @param seconds The number of seconds to sleep before waking up (default = 0).  
 */  
void heltec_deep_sleep(int seconds) {  
  #ifdef WiFi_h  
    WiFi.disconnect(true);  
  #endif  
  
  // Turn off display  
  #ifndef HELTEC_NO_DISPLAY  
    #ifdef ARDUINO_heltec_wireless_tracker  
      heltec_tft_power(false);  
    #elif defined(ARDUINO_heltec_wifi_32_lora_V3)  
      display.displayOff();  
    #else  
      // Adafruit SSD1306 for V3.2 and Wireless Stick  
      display.ssd1306_command(SSD1306_DISPLAYOFF);  
    #endif  
  #endif  
  
  // Put radio to sleep  
  #ifndef HELTEC_NO_RADIOLIB  
    radio.begin();  
    radio.sleep(false); // 'false' for no warm start  
  #endif  
  
  // Turn off external power  
  heltec_ve(false);  
  
  // Turn off LED  
  heltec_led(0);  
  
  // Set all common pins to input to save power  
  pinMode(VBAT_CTRL, INPUT);  
  pinMode(VBAT_ADC, INPUT);  
  pinMode(DIO1, INPUT);  
  pinMode(RST_LoRa, INPUT);  
  pinMode(BUSY_LoRa, INPUT);  
  pinMode(SS, INPUT);  
  pinMode(MISO, INPUT);  
  pinMode(MOSI, INPUT);  
  pinMode(SCK, INPUT);  
  
  // Board-specific pin shutdown  
  #ifdef ARDUINO_heltec_wireless_tracker  
    pinMode(TFT_CS, INPUT);  
    pinMode(TFT_RST, INPUT);  
    pinMode(TFT_DC, INPUT);  
    pinMode(TFT_SCLK, INPUT);  
    pinMode(TFT_MOSI, INPUT);  
    pinMode(TFT_VTFT, INPUT);  
    pinMode(TFT_LED, INPUT);  
    pinMode(GNSS_RX, INPUT);  
    pinMode(GNSS_TX, INPUT);  
  #elif !defined(HELTEC_NO_DISPLAY)  
    // For all boards with OLED displays  
    pinMode(SDA_OLED, INPUT);  
    pinMode(SCL_OLED, INPUT);  
    pinMode(RST_OLED, INPUT);  
  #endif  

  // Sleep GNSS if enabled  
  #ifdef HELTEC_GNSS  
    heltec_gnss_sleep();  
  #endif  
  
  // Set button wakeup if applicable  
  #ifdef HELTEC_POWER_BUTTON  
    esp_sleep_enable_ext0_wakeup(BUTTON, LOW);  
    button.waitForRelease();  
  #endif  
  
  // Set timer wakeup if applicable  
  if (seconds > 0) {  
    esp_sleep_enable_timer_wakeup((int64_t)seconds * 1000000);  
  }  
  
  // Start deep sleep  
  esp_deep_sleep_start();  
}  

/**  
 * @brief Calculates the battery percentage based on the measured voltage.  
 *  
 * @param vbat The battery voltage in volts (default = -1, will measure).  
 * @return The battery percentage (0-100).  
 */  
int heltec_battery_percent(float vbat) {  
  if (vbat == -1) {  
    vbat = heltec_vbat();  
  }  
  for (int n = 0; n < sizeof(scaled_voltage); n++) {  
    float step = (max_voltage - min_voltage) / 256;  
    if (vbat > min_voltage + (step * scaled_voltage[n])) {  
      return 100 - n;  
    }  
  }  
  return 0;  
}  

/**  
 * @brief Checks if the device woke up from deep sleep due to button press.  
 *   
 * @return True if the wake-up cause is a button press, false otherwise.  
 */  
bool heltec_wakeup_was_button() {  
  return esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0;  
}  

/**  
 * @brief Checks if the device woke up from deep sleep due to a timer.  
 *   
 * @return True if the wake-up cause is a timer interrupt, false otherwise.  
 */  
bool heltec_wakeup_was_timer() {  
  return esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER;  
}  

/**  
 * @brief Measures ESP32 chip temperature  
 *   
 * @return float with temperature in degrees celsius.  
 */  
float heltec_temperature() {  
  // Use Arduino's built-in temperature function instead of ESP-IDF API  
  // This is available on all ESP32 variants and avoids header inclusion issues  
  return temperatureRead();  
}  
/**  
 * @brief Controls the display power.  
 *   
 * @param on True to enable, false to disable  
 */  
void heltec_display_power(bool on) {  
  #ifndef HELTEC_NO_DISPLAY  
    #ifdef ARDUINO_heltec_wireless_tracker  
      if (on) {  
        heltec_ve(true);  
        heltec_tft_power(true);  
        pinMode(TFT_RST, OUTPUT);  
        digitalWrite(TFT_RST, HIGH);  
        delay(1);  
        digitalWrite(TFT_RST, LOW);  
        delay(20);  
        digitalWrite(TFT_RST, HIGH);  
      } else {  
        heltec_tft_power(false);  
      }  
    #else  
      if (on) {  
        // For all boards with OLED displays, control via VEXT  
        pinMode(VEXT, OUTPUT);  
        digitalWrite(VEXT, LOW);  // LOW enables power  
        delay(100);  // Give it time to power up  
        
        // Initialize reset pin for OLED  
        pinMode(RST_OLED, OUTPUT);  
        digitalWrite(RST_OLED, HIGH);  
        delay(1);  
        digitalWrite(RST_OLED, LOW);  
        delay(10);  
        digitalWrite(RST_OLED, HIGH);  
        delay(10);  
      } else {  
        #ifdef ARDUINO_heltec_wifi_32_lora_V3  
          // For V3, we use ThingPulse's power down command  
          display.displayOff();  
        #elif defined(BOARD_HELTEC_V3_2) || defined(ARDUINO_heltec_wireless_stick)  
          // For V3.2 and Stick using Adafruit library  
          display.ssd1306_command(SSD1306_DISPLAYOFF);  
        #endif  
        
        // For all boards, turn off VEXT  
        digitalWrite(VEXT, HIGH);  // HIGH disables power  
      }  
    #endif  
  #endif  
}  

/**  
 * @brief Clears the display and sets text properties.  
 *  
 * @param textSize Text size (default = 1).  
 * @param rotation Display rotation (default = 0).  
 */  
void heltec_clear_display(uint8_t textSize, uint8_t rotation) {  
  #ifndef HELTEC_NO_DISPLAY  
    #ifdef ARDUINO_heltec_wireless_tracker  
      display.fillScreen(ST7735_BLACK);  
      display.setTextColor(ST7735_WHITE);  
      display.setCursor(0, 0);  
      display.setRotation(rotation);  
      display.setTextSize(textSize);  
    #elif defined(ARDUINO_heltec_wifi_32_lora_V3)  
      // ThingPulse library for V3  
      display.clear();  
      display.setColor(WHITE);  
      display.setTextAlignment(TEXT_ALIGN_LEFT);  
      if (textSize == 1) display.setFont(ArialMT_Plain_10);  
      else if (textSize == 2) display.setFont(ArialMT_Plain_16);  
      else display.setFont(ArialMT_Plain_24);  
    #else  
      // Adafruit SSD1306 library for V3.2 and Wireless Stick  
      display.clearDisplay();  
      display.setTextSize(textSize);  
      display.setTextColor(SSD1306_WHITE);  
      display.setCursor(0, 0);  
      display.display();  // Already has display update for V3.2  
      _display_needs_update = false;  // Reset flag since we just updated  
    #endif  
  #endif  
}  

/**  
 * @brief Initializes the Heltec library.  
 *  
 * This function should be the first thing in setup() of your sketch.  
 */  
void heltec_setup() {  
  Serial.begin(115200);  
  delay(100);  
  
  // Print board information  
  Serial.println();  
  Serial.println("Heltec ESP32 LoRa Board Library");  
  
  #if defined(ARDUINO_heltec_wifi_32_lora_V3)  
    Serial.println("Board: WiFi LoRa 32 V3");  
  #elif defined(BOARD_HELTEC_V3_2)  
    Serial.println("Board: WiFi LoRa 32 V3.2");  
  #elif defined(ARDUINO_heltec_wireless_stick)  
    Serial.println("Board: Wireless Stick");  
  #elif defined(ARDUINO_heltec_wireless_stick_lite)  
    Serial.println("Board: Wireless Stick Lite");  
  #elif defined(ARDUINO_heltec_wireless_tracker)  
    Serial.println("Board: Wireless Tracker");  
  #else  
    Serial.println("Board: Unknown (using V3.2 defaults)");  
  #endif  
  
  // Initialize SPI for radio  
  #ifdef ARDUINO_heltec_wireless_tracker  
    hspi->begin(SCK, MISO, MOSI, SS);  
  #endif  
  
    // Initialize RadioLib module  
  #ifndef HELTEC_NO_RADIOLIB  
    int radioStatus = radio.begin();  
    if (radioStatus != RADIOLIB_ERR_NONE) {  
      Serial.printf("Radio initialization failed with code %d\n", radioStatus);  
    } else {  
      Serial.println("Radio initialized successfully");  
      
      // Set default radio parameters  
      #if defined(ARDUINO_heltec_wireless_stick) || defined(ARDUINO_heltec_wireless_stick_lite)  
        // SX1276 default parameters for Stick and Stick Lite  
        radio.setFrequency(915.0);  // or 915.0 for US regions  
        radio.setBandwidth(125.0);  
        radio.setSpreadingFactor(9);  
        radio.setCodingRate(5);  
        radio.setSyncWord(0x12);  
      #else  
        // SX1262 default parameters for V3, V3.2, and Tracker  

      #endif  
    }  
  #endif  

  // Initialize display  
  #ifndef HELTEC_NO_DISPLAY  
    #ifdef ARDUINO_heltec_wireless_tracker  
      heltec_display_power(true);  
      display.initR(INITR_MINI160x80); // Initialize ST7735  
      display.fillScreen(ST7735_BLACK);  
      display.setTextColor(ST7735_WHITE);  
      display.setTextSize(1);  
      display.setTextWrap(true);  
    #elif defined(ARDUINO_heltec_wifi_32_lora_V3)  
      // ThingPulse SSD1306 for V3  
      heltec_display_power(true);  
      Wire.begin(SDA_OLED, SCL_OLED);  
      display.init();  
      display.setContrast(255);  
      display.flipScreenVertically();  
      display.clear();  
    #else  
      // For V3.2 and Wireless Stick - Using the known working code approach  
      // Power on the OLED display  
      heltec_display_power(true);  
      
      // Initialize I2C  
      Wire.begin(SDA_OLED, SCL_OLED);  
      
      // Initialize display  
      if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {  
        Serial.println("SSD1306 allocation failed");  
      } else {  
        Serial.println("OLED initialized successfully");  
      }  
      
      // Clear the display buffer  
      display.clearDisplay();  
      display.setTextSize(1);  
      display.setTextColor(SSD1306_WHITE);  
      display.setCursor(0, 0);  
    #endif  
    
    // Display welcome message  
    #if defined(ARDUINO_heltec_wifi_32_lora_V3)  
      display.clear();  
      display.setFont(ArialMT_Plain_10);  
      display.setTextAlignment(TEXT_ALIGN_LEFT);  
      display.drawString(0, 0, "Heltec ESP32 LoRa");  
      display.drawString(0, 16, "WiFi LoRa 32 V3");  
      display.display();  
    #elif defined(BOARD_HELTEC_V3_2)  
      display.clearDisplay();  
      display.setTextSize(1);  
      display.setTextColor(SSD1306_WHITE);  
      display.setCursor(0, 0);  
      display.println("Heltec ESP32 LoRa");  
      display.println("WiFi LoRa 32 V3.2");  
      display.display();  
    #elif defined(ARDUINO_heltec_wireless_stick)  
      display.clearDisplay();  
      display.setTextSize(1);  
      display.setCursor(0, 0);  
      display.println("Heltec");  
      display.println("Wireless Stick");  
      display.display();  
    #elif defined(ARDUINO_heltec_wireless_tracker)  
      display.fillScreen(ST7735_BLACK);  
      display.setCursor(0, 0);  
      display.println("Heltec");  
      display.println("Wireless Tracker");  
    #endif  
  #endif  

  // Initialize the built-in LED  
  // Configure LED PWM  
  ledcSetup(LED_CHAN, 5000, 8);  
  ledcAttachPin(LED_PIN, LED_CHAN);  
  ledcWrite(LED_CHAN, 0); // Turn off initially  

  // Initialize the button  
  #ifdef HELTEC_POWER_BUTTON  
    pinMode(BUTTON, INPUT);  
  #endif  

  delay(2000); // Display welcome message  

  // WiFi initialization removed - handled by wifi_helper library now
}  

bool heltec_button_clicked() {  
  bool result = buttonClicked;  
  buttonClicked = false;  // Clear after reading  
  return result;  
}  

// For backward compatibility - allows delay with power button check  
void heltec_delay(int ms) {  
  uint32_t start = millis();  
  while (millis() - start < ms) {  
    heltec_loop();  
    delay(10);  
  }  
}  

// Internal interrupt handler for packet reception  
void _handleLoRaRx() {  
  _packetReceived = true;  
}  

/**  
 * @brief Subscribe to LoRa packet reception events  
 *   
 * @param callback Function to be called when a packet is received  
 * @return true if subscription was successful  
 */  
bool heltec_subscribe_packets(PacketCallback callback) {  
  #ifndef HELTEC_NO_RADIOLIB  
    _packetCallback = callback;
    
    // Clear any previous action first
    radio.clearDio1Action();
    delay(10);  // Give it time to clear
    
    // Set up the new action
    radio.setDio1Action(_handleLoRaRx);
    delay(10);  // Let the interrupt setup complete
    
    // Start receive
    int state = radio.startReceive();
    delay(30);  // Give it more time to enter receive mode

    if (state == RADIOLIB_ERR_NONE) {  
      Serial.println("LoRa packet subscription active");  
      return true;  
    } else {  
      Serial.printf("Failed to start radio receiver: %d\n", state);  
      return false;  
    }  
  #else  
    Serial.println("Radio not available - packet subscription failed");  
    return false;  
  #endif  
}  
/**
 * @brief Unsubscribe from LoRa packet reception events
 * 
 * @return true if unsubscription was successful
 */
bool heltec_unsubscribe_packets() {
  #ifndef HELTEC_NO_RADIOLIB
    // Clear the callback
    _packetCallback = NULL;
    
    // Clear the interrupt
    radio.clearDio1Action();
    delay(20); 
    // Put radio in standby mode
    int state = radio.standby();
    delay(20);
    
    if (state == RADIOLIB_ERR_NONE) {
      Serial.println("LoRa packet subscription disabled");
      return true;
    } else {
      Serial.printf("Failed to put radio in standby: %d\n", state);
      return false;
    }
  #else
    Serial.println("Radio not available - unsubscription not needed");
    return false;
  #endif
}

/**
 * @brief Process any received packets in the main loop
 * 
 * This should be called in every iteration of loop()
 * It will invoke the callback when packets are received
 */
void heltec_process_packets() {
  #ifndef HELTEC_NO_RADIOLIB
    if (_packetReceived) {
      // Clear flag immediately to prevent race conditions
      _packetReceived = false;
      
      // Temporarily disable interrupt to prevent recursion
      radio.clearDio1Action();
      
      // Read the packet data
      _packetData = "";
      int state = radio.readData(_packetData);
      
      if (state == RADIOLIB_ERR_NONE) {
        // Get signal quality information
        _packetRssi = radio.getRSSI();
        _packetSnr = radio.getSNR();
        
        // Invoke the callback if registered
        if (_packetCallback) {
          _packetCallback(_packetData, _packetRssi, _packetSnr);
        }
      } else {
        Serial.printf("Error reading LoRa packet: %d\n", state);
      }
      
      // Make sure radio is in a clean state before restarting reception
      radio.standby();
      delay(10);
      
      // Restart reception
      radio.startReceive();
      delay(10);
      
      // Re-enable interrupt handler AFTER reception has started
      radio.setDio1Action(_handleLoRaRx);
    }
  #endif
}

/**
 * @brief The main loop for the Heltec library.
 * 
 * This function should be called in the loop() of your sketch
 * to handle button debouncing and other periodic tasks.
 */
void heltec_loop() {
  // Handle display updates for V3.2
  heltec_display_update();
  
  // Update button state
  button.update();
  
  // Check for a single click
  if (button.isSingleClick()) {
    buttonClicked = true;
  }
  
  // Handle power button functionality
  #ifdef HELTEC_POWER_BUTTON
    if (button.pressedFor(2000)) {
      both.println("\nSleeping...");
      delay(500);  // Let the message display
      heltec_deep_sleep();
    }
  #endif
  
  // Handle GNSS updates
  #ifdef HELTEC_GNSS
    heltec_gnss_update();
  #endif

  // Process any pending packet receptions
  heltec_process_packets();
}