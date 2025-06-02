/**  
 * @file heltec_unofficial.cpp  
 * @brief Implementation file for the Heltec ESP32 LoRa library.  
 */  

#include "heltec_unofficial.h"  

// ====== Constants ======
// Radio configuration
#define HELTEC_LORA_FREQ    915.0  // MHz, 868.0 for EU regions
#define HELTEC_LORA_BW      125.0  // kHz
#define HELTEC_LORA_SF      9      // Spreading factor
#define HELTEC_LORA_CR      5      // Coding rate
#define HELTEC_LORA_SYNC    0x12   // Sync word

// Chip-specific settings
#define HELTEC_SX1262_POWER   14.0    // dBm
#define HELTEC_SX1262_CURRENT 140.0   // mA
#define HELTEC_SX1276_POWER   17      // Different scale for SX1276

// Battery calibration  
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

// ====== Global variables ======
// Button related
HotButton button(BUTTON);  
bool buttonClicked = false;  

// Display update flag (for V3.2)  
bool _display_needs_update = false;  

// RadioLib instances based on board type  
#ifndef HELTEC_NO_RADIOLIB  
  #if defined(ARDUINO_heltec_wireless_stick) || defined(ARDUINO_heltec_wireless_stick_lite)  
    SX1276 radio = new Module(SS, DIO1, RST_LoRa, BUSY_LoRa);  
  #elif defined(ARDUINO_heltec_wireless_tracker)  
    SPIClass* hspi = new SPIClass(HSPI);  
    SX1262 radio = new Module(SS, DIO1, RST_LoRa, BUSY_LoRa, *hspi);  
  #else  
    SX1262 radio = new Module(SS, DIO1, RST_LoRa, BUSY_LoRa);  
  #endif  
#endif  

// Display instances based on board type  
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
  Print &both = Serial;  
#endif  

// GNSS related
#ifdef HELTEC_GNSS  
  TinyGPSPlus gps;  
  HardwareSerial gpsSerial = HardwareSerial(1);  
#endif  

// Packet subscription system  
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

// ====== Internal helper functions ======
// Interrupt handler for packet reception  
void _handleLoRaRx() {  
  _packetReceived = true;  
}  

// ====== Public API implementations ======
/**  
 * @brief Gets a string describing the current board  
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
 * @param state The state of the VEXT pin (true = enable, false = disable).  
 */  
void heltec_ve(bool state) {  
  if (state) {  
    pinMode(VEXT, OUTPUT);  
    digitalWrite(VEXT, LOW);  
    delay(100);  
  } else {  
    pinMode(VEXT, INPUT);  
  }  
}  

/**  
 * @brief Controls the TFT power and backlight for Wireless Tracker.  
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

/**  
 * @brief Calculates the battery percentage based on the measured voltage.  
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
 * @brief Measures ESP32 chip temperature  
 * @return float with temperature in degrees celsius.  
 */  
float heltec_temperature() {  
  return temperatureRead();  
}  

/**  
 * @brief Checks if the device woke up from deep sleep due to button press.  
 * @return True if the wake-up cause is a button press, false otherwise.  
 */  
bool heltec_wakeup_was_button() {  
  return esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0;  
}  

/**  
 * @brief Checks if the device woke up from deep sleep due to a timer.  
 * @return True if the wake-up cause is a timer interrupt, false otherwise.  
 */  
bool heltec_wakeup_was_timer() {  
  return esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER;  
}  

/**  
 * @brief Checks if the button was clicked.  
 * @return True if the button was clicked, false otherwise.  
 */  
bool heltec_button_clicked() {  
  bool result = buttonClicked;  
  buttonClicked = false;  // Clear after reading  
  return result;  
}  

/**  
 * @brief For backward compatibility - allows delay with power button check  
 * @param ms Milliseconds to delay  
 */  
void heltec_delay(int ms) {  
  uint32_t start = millis();  
  while (millis() - start < ms) {  
    heltec_loop();  
    delay(10);  
  }  
}  

// ====== GNSS functions ======
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
  }  

  /**  
   * @brief Updates GNSS data if available  
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

// ====== Display functions ======
/**  
 * @brief Controls the display power.  
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
          display.displayOff();  
        #elif defined(BOARD_HELTEC_V3_2) || defined(ARDUINO_heltec_wireless_stick)  
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
      display.clear();  
      display.setColor(WHITE);  
      display.setTextAlignment(TEXT_ALIGN_LEFT);  
      if (textSize == 1) display.setFont(ArialMT_Plain_10);  
      else if (textSize == 2) display.setFont(ArialMT_Plain_16);  
      else display.setFont(ArialMT_Plain_24);  
    #else  
      display.clearDisplay();  
      display.setTextSize(textSize);  
      display.setTextColor(SSD1306_WHITE);  
      display.setCursor(0, 0);  
      display.display();  
      _display_needs_update = false;  
    #endif  
  #endif  
}  

// ====== Radio functions ======

/**  
 * @brief Subscribe to LoRa packet reception events  
 * @param callback Function to be called when a packet is received  
 * @return true if subscription was successful  
 */  
bool heltec_subscribe_packets(PacketCallback callback) {  
  #ifndef HELTEC_NO_RADIOLIB  
    // Validate input
    if (callback == NULL) {
      Serial.println("Error: NULL callback provided");
      return false;
    }
    
    // Store the callback
    _packetCallback = callback;
    
    // Reset received flag to ensure clean state
    _packetReceived = false;
    
    // Clear any previous action first
    radio.clearDio1Action();
    delay(10);
    
    // Set up the new action
    radio.setDio1Action(_handleLoRaRx);
    delay(10);
    
    // Start receive
    int state = radio.startReceive();
    delay(10);

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
 * @return true if unsubscription was successful
 */
bool heltec_unsubscribe_packets() {
  #ifndef HELTEC_NO_RADIOLIB
    // Clear the callback
    _packetCallback = NULL;
    
    // Reset received flag to ensure clean state
    _packetReceived = false;
    
    // Clear the interrupt
    radio.clearDio1Action();
    delay(10); 

    // Put radio in standby mode
    int state = radio.standby();
    delay(10);
    
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
      
      // Only restart reception if we still have a callback (user might have unsubscribed in callback)
      if (_packetCallback) {
        // Restart reception
        radio.startReceive();
        delay(10);
        
        // Re-enable interrupt handler AFTER reception has started
        radio.setDio1Action(_handleLoRaRx);
      }
    }
  #endif
}

// ====== Power management ======
/**  
 * @brief Puts the device into deep sleep mode.  
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

// ====== Main functions ======
/**  
 * @brief Initializes the Heltec library.  
 */  
void heltec_setup() {  
  Serial.begin(115200);  
  delay(100);  
  
  // Initialize SPI for radio  
  #ifdef ARDUINO_heltec_wireless_tracker  
    hspi->begin(SCK, MISO, MOSI, SS);  
  #endif  
  
  // Initialize display
  #ifndef HELTEC_NO_DISPLAY  
    #ifdef ARDUINO_heltec_wireless_tracker  
      // Initialize TFT display for Wireless Tracker
      heltec_display_power(true);  
      display.initR(INITR_MINI160x80);
      display.fillScreen(ST7735_BLACK);  
      display.setTextColor(ST7735_WHITE);  
      display.setTextSize(1);  
      display.setTextWrap(true);
      
    #elif defined(ARDUINO_heltec_wifi_32_lora_V3)  
      // Initialize OLED for V3 - ThingPulse library
      heltec_display_power(true);  
      Wire.begin(SDA_OLED, SCL_OLED);  
      display.init();  
      display.setContrast(255);  
      display.flipScreenVertically();
      
    #else  
      // Initialize OLED for V3.2 and Wireless Stick - Adafruit library
      heltec_display_power(true);  
      Wire.begin(SDA_OLED, SCL_OLED);  
      
      if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {  
        Serial.println("SSD1306 allocation failed");  
      } else {  
        Serial.println("OLED initialized successfully");
      }
    #endif
    
    // Clear display and show welcome message
    heltec_clear_display();
    both.println("Heltec ESP32 LoRa");
    both.println(heltec_get_board_name());
    
    // ThingPulse library (V3) needs explicit display update that isn't handled by PrintSplitter
    #if defined(ARDUINO_heltec_wifi_32_lora_V3)
      display.display();
    #endif
  #else
    // No display - just log to Serial
    Serial.println("Heltec ESP32 LoRa");  
    Serial.println(heltec_get_board_name());
  #endif
  
  // Initialize RadioLib module  
  #ifndef HELTEC_NO_RADIOLIB  
    int radioStatus = radio.begin();  
    if (radioStatus != RADIOLIB_ERR_NONE) {  
      both.printf("Radio initialization failed with code %d\n", radioStatus);  
    } else {  
      both.println("Radio initialized successfully");  
      
      // Common parameters for all boards
      radio.setFrequency(HELTEC_LORA_FREQ);
      radio.setBandwidth(HELTEC_LORA_BW);
      radio.setSpreadingFactor(HELTEC_LORA_SF);
      radio.setCodingRate(HELTEC_LORA_CR);
      radio.setSyncWord(HELTEC_LORA_SYNC);

      // Chip-specific parameters
      #if defined(ARDUINO_heltec_wireless_stick) || defined(ARDUINO_heltec_wireless_stick_lite)
        radio.setOutputPower(HELTEC_SX1276_POWER);
      #else
        radio.setOutputPower(HELTEC_SX1262_POWER);
        radio.setCurrentLimit(HELTEC_SX1262_CURRENT);
      #endif
    }  
  #endif  

  // Initialize the built-in LED  
  ledcSetup(LED_CHAN, 5000, 8);  
  ledcAttachPin(LED_PIN, LED_CHAN);  
  ledcWrite(LED_CHAN, 0); // Turn off initially  

  // Initialize the button  
  #ifdef HELTEC_POWER_BUTTON  
    pinMode(BUTTON, INPUT);  
  #endif  

  delay(2000); // Let welcome message be visible
}

/**
 * @brief The main loop for the Heltec library.
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