/**  
 * @file heltec_unofficial.h  
 * @brief Header file for the Heltec ESP32 LoRa library.  
 *  
 * This library provides functions for controlling Heltec ESP32 LoRa boards,  
 * including LED brightness, battery voltage, deep sleep, display, and LoRa radio.  
 * Supported boards:   
 * - WiFi LoRa 32 V3  
 * - WiFi LoRa 32 V3.2 (added May 2025 by @nolfonzo)  
 * - Wireless Stick  
 * - Wireless Stick Lite  
 * - Wireless Tracker V1.1 (added May 2025 by @nolfonzo)  
 *  
 * @author Original: @ropg  
 * @author Additions: @nolfonzo (May 2025)  
 */  

#ifndef HELTEC_UNOFFICIAL_H
#define HELTEC_UNOFFICIAL_H

#include <Arduino.h>   
#include <HotButton.h>

#define HELTEC_UNOFFICIAL_VERSION "1.0.0"

// Optional power button feature (long press for sleep)
// Uncomment to enable power button functionality
#define HELTEC_POWER_BUTTON

// Define V3.2 manually since it doesn't have an Arduino board definition  
#if !defined(ARDUINO_heltec_wifi_32_lora_V3) && \
    !defined(ARDUINO_heltec_wireless_stick) && \
    !defined(ARDUINO_heltec_wireless_stick_lite) && \
    !defined(ARDUINO_heltec_wireless_tracker) && \
    !defined(BOARD_HELTEC_V3_2)  
  // No specific board detected - default to V3.2  
  #define BOARD_HELTEC_V3_2  
#endif  

// Radio configuration constants
#define HELTEC_LORA_FREQ    915.0  // MHz, 868.0 for EU regions
#define HELTEC_LORA_BW      125.0  // kHz
#define HELTEC_LORA_SF      9      // Spreading factor
#define HELTEC_LORA_CR      5      // Coding rate
#define HELTEC_LORA_SYNC    0x12   // Sync word

// Chip-specific settings
#define HELTEC_SX1262_POWER   14.0    // dBm
#define HELTEC_SX1262_CURRENT 140.0   // mA
#define HELTEC_SX1276_POWER   17      // Different scale for SX1276

// Flag to track if display needs updating (for V3.2)  
extern bool _display_needs_update;  

// Common pin definitions for all boards  
#define BUTTON    GPIO_NUM_0  // 'PRG' Button  
// HotButton instance for the PRG button  
extern HotButton button; 
#define LED_PIN   GPIO_NUM_35 // Onboard LED  
#define LED_FREQ  5000        // LED PWM frequency   
#define LED_CHAN  0           // LED PWM channel  
#define LED_RES   8           // LED PWM resolution  

// Common pins for V3, V3.2, and Tracker (S3-based boards)  
#if defined(ARDUINO_heltec_wifi_32_lora_V3) || defined(BOARD_HELTEC_V3_2) || defined(ARDUINO_heltec_wireless_tracker)  
  #define VEXT      GPIO_NUM_36 // External power control  
  #define VBAT_CTRL GPIO_NUM_37 // Battery voltage measurement control  
  #define VBAT_ADC  GPIO_NUM_1  // Battery voltage ADC pin  
  // SPI & Radio pins (SX1262) - common across all S3 boards  
  #define SS        GPIO_NUM_8  
  #define DIO1      GPIO_NUM_14  
  #define RST_LoRa  GPIO_NUM_12  
  #define BUSY_LoRa GPIO_NUM_13  
  #define MOSI      GPIO_NUM_10  
  #define MISO      GPIO_NUM_11  
  #define SCK       GPIO_NUM_9  
#endif  

// Common pins for Wireless Stick and Stick Lite (S2-based boards)  
#if defined(ARDUINO_heltec_wireless_stick) || defined(ARDUINO_heltec_wireless_stick_lite)  
  #define VEXT      GPIO_NUM_21 // External power control  
  #define VBAT_CTRL GPIO_NUM_37 // Battery voltage measurement control  
  #define VBAT_ADC  GPIO_NUM_1  // Battery voltage ADC pin  
  // SPI & Radio pins (SX1276)  
  #define SS        GPIO_NUM_18  
  #define DIO1      GPIO_NUM_35  
  #define RST_LoRa  GPIO_NUM_14  
  #define BUSY_LoRa GPIO_NUM_33  
  #define MOSI      GPIO_NUM_27  
  #define MISO      GPIO_NUM_19  
  #define SCK       GPIO_NUM_5  
#endif  

// Board-specific pin definitions (only what's unique to each board)  
#if defined(ARDUINO_heltec_wifi_32_lora_V3) || defined(BOARD_HELTEC_V3_2)  
  // OLED display pins for V3 and V3.2  
  #define SDA_OLED  GPIO_NUM_17 // OLED SDA  
  #define SCL_OLED  GPIO_NUM_18 // OLED SCL  
  #define RST_OLED  GPIO_NUM_21 // OLED reset  
#endif  

#if defined(ARDUINO_heltec_wireless_stick)  
  // OLED display pins for Wireless Stick  
  #define SDA_OLED  GPIO_NUM_4  // OLED SDA  
  #define SCL_OLED  GPIO_NUM_15 // OLED SCL  
  #define RST_OLED  GPIO_NUM_16 // OLED reset  
#endif  

#if defined(ARDUINO_heltec_wireless_tracker)  
  // TFT display pins for Tracker  
  #define TFT_CS    GPIO_NUM_38 // Chip select  
  #define TFT_RST   GPIO_NUM_39 // Reset  
  #define TFT_DC    GPIO_NUM_40 // Data/Command  
  #define TFT_SCLK  GPIO_NUM_41 // SPI Clock  
  #define TFT_MOSI  GPIO_NUM_42 // SPI MOSI  
  #define TFT_VTFT  GPIO_NUM_3  // Power control  
  #define TFT_LED   GPIO_NUM_21 // Backlight control  
  #define TFT_MISO  -1          // Not used  
  // GNSS pins (UC6580)  
  #define GNSS_RX   GPIO_NUM_34  
  #define GNSS_TX   GPIO_NUM_33  
#endif  

// Include libraries based on configuration  
#ifdef HELTEC_GNSS  
  #include <TinyGPSPlus.h>  
  // GNSS global variables
  extern TinyGPSPlus gps;
  extern HardwareSerial gpsSerial;
#endif  

#ifndef HELTEC_NO_RADIOLIB  
  #include "RadioLib.h"  
  // Make sure the power off button works when using RADIOLIB_OR_HALT  
  #define RADIOLIB_DO_DURING_HALT delay(10)  
  #include "RadioLib_convenience.h"  

  // RadioLib instances based on board type  
  #if defined(ARDUINO_heltec_wireless_stick) || defined(ARDUINO_heltec_wireless_stick_lite)  
    // Stick and Stick Lite use SX1276  
    extern SX1276 radio;  
  #elif defined(ARDUINO_heltec_wireless_tracker)  
    // Tracker needs HSPI for LoRa (VSPI used for TFT)  
    #include <SPI.h>  
    extern SPIClass* hspi;
    extern SX1262 radio;  
  #else  
    // V3 and V3.2 use SX1262  
    extern SX1262 radio;  
  #endif  
#endif

/**  
 * @class PrintSplitter  
 * @brief A class that splits the output of the Print class to two different  
 *        Print objects.  
 */  
class PrintSplitter : public Print {  
  public:  
    PrintSplitter(Print &_a, Print &_b) : a(_a), b(_b) {}  
    size_t write(uint8_t c);
    size_t write(const uint8_t *buffer, size_t size);
  private:  
    Print &a;  
    Print &b;  
};  

// Display instances based on board type  
#ifndef HELTEC_NO_DISPLAY  
  #ifdef ARDUINO_heltec_wireless_tracker  
    // ST7735 TFT display for Tracker  
    #include <Adafruit_GFX.h>  
    #include <Adafruit_ST7735.h>  
    extern Adafruit_ST7735 display;  
    extern PrintSplitter both;  
  #elif defined(ARDUINO_heltec_wifi_32_lora_V3)  
    // ThingPulse SSD1306 library for V3  
    #include "SSD1306Wire.h"  
    #include "OLEDDisplayUi.h"  
    #define DISPLAY_GEOMETRY GEOMETRY_128_64  
    extern SSD1306Wire display;  
    extern PrintSplitter both;  
  #else  
    // Adafruit SSD1306 for V3.2 and Wireless Stick  
    #include <Adafruit_GFX.h>  
    #include <Adafruit_SSD1306.h>  
    #ifdef ARDUINO_heltec_wireless_stick  
      #define SCREEN_WIDTH 64  
      #define SCREEN_HEIGHT 32  
    #else  
      #define SCREEN_WIDTH 128  
      #define SCREEN_HEIGHT 64  
    #endif  
    extern Adafruit_SSD1306 display;  
    extern PrintSplitter both;  
  #endif  
#else  
  // No display - just use Serial  
  extern Print &both;  
#endif  

// Battery voltage calibration  
extern const float min_voltage;  
extern const float max_voltage;  
extern const uint8_t scaled_voltage[100];  

// Button state tracking
extern bool buttonClicked;

// Packet handling callback type definition
typedef void (*PacketCallback)(String &data, float rssi, float snr);

// Global variables for packet subscription system
extern PacketCallback _packetCallback;
extern volatile bool _packetReceived;
extern String _packetData;
extern float _packetRssi;
extern float _packetSnr;

// ====== Function Declarations ======

// Basic board functions
/**
 * @brief Gets a string describing the current board
 * @return Board description string
 */
const char* heltec_get_board_name();

/**
 * @brief Measures ESP32 chip temperature
 * @return Temperature in degrees celsius
 */
float heltec_temperature();

/**
 * @brief Initializes the Heltec library
 * This function should be the first thing in setup() of your sketch
 */
void heltec_setup();

/**
 * @brief The main loop for the Heltec library
 * This function should be called in loop() of your sketch
 */
void heltec_loop();

/**
 * @brief Delay with power button check
 * @param ms Milliseconds to delay
 */
void heltec_delay(int ms);

// Display functions
/**
 * @brief Updates the display buffer to the screen
 * Necessary for V3.2 boards using the Adafruit SSD1306 library
 */
void heltec_display_update();

/**
 * @brief Controls the display power
 * @param on True to enable, false to disable
 */
void heltec_display_power(bool on);

/**
 * @brief Clears the display and sets text properties
 * @param textSize Text size (default = 1)
 * @param rotation Display rotation (default = 0)
 */
void heltec_clear_display(uint8_t textSize = 1, uint8_t rotation = 0);

// Power management functions
/**
 * @brief Controls the LED brightness
 * @param percent The brightness percentage (0-100)
 */
void heltec_led(int percent);

/**
 * @brief Controls the VEXT pin (external power)
 * @param state True to enable, false to disable
 */
void heltec_ve(bool state);

/**
 * @brief Controls the TFT power and backlight for Wireless Tracker
 * @param state True to enable, false to disable
 */
void heltec_tft_power(bool state);

/**
 * @brief Measures the battery voltage
 * @return The battery voltage in volts
 */
float heltec_vbat();

/**
 * @brief Calculates the battery percentage
 * @param vbat The battery voltage or -1 to measure
 * @return The battery percentage (0-100)
 */
int heltec_battery_percent(float vbat = -1);

/**
 * @brief Puts the device into deep sleep mode
 * @param seconds The number of seconds to sleep (0 = indefinite)
 */
void heltec_deep_sleep(int seconds = 0);

/**
 * @brief Checks if wakeup was caused by button press
 * @return True if wakeup was from button press
 */
bool heltec_wakeup_was_button();

/**
 * @brief Checks if wakeup was caused by timer
 * @return True if wakeup was from timer
 */
bool heltec_wakeup_was_timer();

// Button handling
/**
 * @brief Checks if the button was clicked
 * @return True if button was clicked (clears after reading)
 */
bool heltec_button_clicked();

// GNSS functions
#ifdef HELTEC_GNSS
/**
 * @brief Initializes the GNSS module
 */
void heltec_gnss_begin();

/**
 * @brief Puts the GNSS module to sleep
 */
void heltec_gnss_sleep();

/**
 * @brief Updates GNSS data if available
 * @return True if new data was processed
 */
bool heltec_gnss_update();
#endif

// Packet handling functions
/**
 * @brief Internal callback handler for LoRa interrupts
 * (Not called directly by user code)
 */
void _handleLoRaRx();

/**
 * @brief Subscribe to LoRa packet reception events
 * @param callback Function to be called when a packet is received
 * @return true if subscription was successful
 */
bool heltec_subscribe_packets(PacketCallback callback);

/**
 * @brief Unsubscribe from LoRa packet reception events
 * @return true if unsubscription was successful
 */
bool heltec_unsubscribe_packets();

/**
 * @brief Process any received packets in the main loop
 * This should be called in every iteration of loop()
 */
void heltec_process_packets();

#endif // HELTEC_UNOFFICIAL_H