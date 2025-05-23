#include <Arduino.h>


#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include <SPI.h>             // SPI library for communication

// Pin definitions for Heltec Wireless Tracker V1.1 TFT
#define TFT_CS    38  // Chip select
#define TFT_REST  39  // Reset
#define TFT_DC    40  // Data/Command
#define TFT_SCLK  41  // SPI Clock
#define TFT_MOSI  42  // SPI MOSI
#define TFT_VTFT  3   // Power control
#define TFT_LED   21  // Backlight control

// Initialize ST7735 display
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_REST);

void setup() {
  // Power control (GPIO3)
  pinMode(TFT_VTFT, OUTPUT);
  digitalWrite(TFT_VTFT, HIGH);

  // Backlight control (GPIO21)
  pinMode(TFT_LED, OUTPUT);
  digitalWrite(TFT_LED, HIGH);

  // Initialize display
  tft.initR(INITR_MINI160x80);
  tft.setRotation(1); // Rotated right (160x80, likely right-side up)
  tft.fillScreen(ST7735_BLACK);

  // Write text
  tft.setTextColor(ST7735_WHITE);
  tft.setTextSize(2); // Larger text
  tft.setCursor(0, 0);
  tft.println("Hello World");

  // Serial confirmation
  Serial.begin(115200);
  delay(1000);
  Serial.println("Displayed 'Hello World' with rotation 1, size 2");
}

void loop() {
  // No loop action needed
}