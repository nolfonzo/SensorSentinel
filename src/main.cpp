// Define Tracker Board
#define HELTEC_WIRELESS_TRACKER

#include <heltec_unofficial.h>

void setup() {
  heltec_setup(); // Initialize Serial, SPI, and TFT (sets TFT_VTFT, TFT_LED, and TFT_RST)

  // Customize TFT settings
  display.setRotation(1); // 160x80 TFT, right-side up
  display.setTextSize(2); // Larger text
  display.setCursor(0, 0);

  // Print to both Serial and TFT
  both.println("Hello World");
  Serial.println("Displayed 'Hello World' on TFT with rotation 1, size 2");

  // Debug power pins
  Serial.print("TFT_VTFT: ");
  Serial.println(digitalRead(TFT_VTFT) ? "HIGH" : "LOW");
  Serial.print("TFT_LED: ");
  Serial.println(digitalRead(TFT_LED) ? "HIGH" : "LOW");
}

void loop() {
  // Empty loop for simple example
}