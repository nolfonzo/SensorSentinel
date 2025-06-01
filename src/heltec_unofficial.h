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

// Define V3.2 manually since it doesn't have an Arduino board definition  
#if !defined(ARDUINO_heltec_wifi_32_lora_V3) && \
    !defined(ARDUINO_heltec_wireless_stick) && \
    !defined(ARDUINO_heltec_wireless_stick_lite) && \
    !defined(ARDUINO_heltec_wireless_tracker) && \
    !defined(BOARD_HELTEC_V3_2)  
  // No specific board detected - default to V3.2  
  #define BOARD_HELTEC_V3_2  
#endif  

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
  // GNSS function declarations
  void heltec_gnss_begin();
  void heltec_gnss_sleep();
  bool heltec_gnss_update();
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

// Function prototypes  
const char* heltec_get_board_name();
void heltec_display_update();  
void heltec_led(int percent);  
void heltec_ve(bool state);  
void heltec_tft_power(bool state);  
float heltec_vbat();  
void heltec_deep_sleep(int seconds = 0);  
int heltec_battery_percent(float vbat = -1);  
bool heltec_wakeup_was_button();  
bool heltec_wakeup_was_timer();  
float heltec_temperature();  
void heltec_display_power(bool on);  
void heltec_clear_display(uint8_t textSize = 1, uint8_t rotation = 0);  
void heltec_setup();  
void heltec_loop();  
bool heltec_button_clicked();
void heltec_delay(int ms);

// Packet handling functions
void _handleLoRaRx();
bool heltec_subscribe_packets(PacketCallback callback);
bool heltec_unsubscribe_packets();
void heltec_process_packets();

#endif // HELTEC_UNOFFICIAL_H