#define HELTEC_WIRELESS_TRACKER
#include <heltec_unofficial.h>

void setup() {
  Serial.begin(115200);
  delay(1000); // Wait for Serial Monitor to connect
  heltec_setup();
  display.setRotation(1); // 160x80, right-side up
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println("Hello World");
  Serial.println("Displayed 'Hello World' on TFT");
}

void loop() {}