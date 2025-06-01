; PlatformIO Project Configuration File
;
;   Build options: build flags, source filter
;   Upload options: custom upload port, speed and extra flags
;   Library options: dependencies, extra library storages
;   Advanced options: extra scripting
;
; Please visit documentation for the other options and examples
; https://docs.platformio.org/page/projectconf.html

[platformio]
default_envs = heltec_wifi_lora_32_V3_2

; Common settings for all Heltec boards
[env]
platform = espressif32
framework = arduino
monitor_speed = 115200
lib_extra_dirs = 
    /Users/nolfonzo/src/esp32/heltec_esp32_lora_v3
    /Users/nolfonzo/src/esp32/HotButton
build_flags =
    -I/Users/nolfonzo/src/esp32/heltec_esp32_lora_v3/src/
lib_deps =
    knolleary/PubSubClient
    bblanchon/ArduinoJson

; Filter to only include tracker_rx_tx.cpp
; build_src_filter = +<tracker_rx_tx.cpp> -<*> +<main.cpp>
; build_src_filter = +<tracker_rx_tx.cpp>
; build_src_filter = +<simpleui.cpp>
; build_src_filter = +<main.cpp>
build_src_filter = +<test_mqtt_gateway.cpp>
; build_src_filter = +<test_send_packet.cpp>

; Heltec WiFi LoRa 32 V3
[env:heltec_wifi_lora_32_V3]
board = heltec_wifi_lora_32_V3
lib_deps = 
    jgromes/RadioLib
    thingpulse/ESP8266 and ESP32 OLED driver for SSD1306 displays

; Heltec WiFi LoRa 32 V3.2
[env:heltec_wifi_lora_32_V3_2]
board = heltec_wifi_lora_32_V3
lib_deps = 
    jgromes/RadioLib
    adafruit/Adafruit SSD1306
    adafruit/Adafruit GFX Library
    adafruit/Adafruit BusIO
    olikraus/U8g2
    knolleary/PubSubClient
    bblanchon/ArduinoJson
build_flags =
    ${env.build_flags}
    -DBOARD_HELTEC_V3_2

; Heltec Wireless Stick
[env:heltec_wireless_stick]
board = heltec_wireless_stick
lib_deps = 
    jgromes/RadioLib
    adafruit/Adafruit SSD1306
    adafruit/Adafruit GFX Library
    adafruit/Adafruit BusIO

; Heltec Wireless Stick Lite
[env:heltec_wireless_stick_lite]
board = heltec_wireless_stick_lite
lib_deps = 
    jgromes/RadioLib
    adafruit/Adafruit BusIO
build_flags =
    ${env.build_flags}
    -DHELTEC_NO_DISPLAY

; Heltec Wireless Tracker
[env:heltec_wireless_tracker]
board = esp32-s3-devkitc-1
lib_deps = 
    jgromes/RadioLib
    adafruit/Adafruit GFX Library
    adafruit/Adafruit ST7735 and ST7789 Library
    mikalhart/TinyGPSPlus
    adafruit/Adafruit BusIO
    knolleary/PubSubClient
    bblanchon/ArduinoJson 
build_flags =
    ${env.build_flags}
    -DHELTEC_GNSS                      ; Enable GNSS support
    -DARDUINO_heltec_wireless_tracker  ; Use Arduino naming convention



#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED display settings
#define VEXT_PIN 36    // External voltage control pin
#define OLED_SDA 17    // OLED I2C SDA pin
#define OLED_SCL 18    // OLED I2C SCL pin
#define OLED_RST 21    // OLED reset pin (not used but needed for constructor)
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C  // I2C address

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST);

void setup() {
  Serial.begin(115200);
  Serial.println("Heltec LoRa 32 v3.2 OLED Test");
  
  // Power on the OLED display
  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, LOW);  // LOW enables power
  delay(100);  // Give it time to power up
  
  // Initialize I2C
  Wire.begin(OLED_SDA, OLED_SCL);
  
  // Initialize display
  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("SSD1306 allocation failed");
    while(1);
  }
  
  Serial.println("OLED initialized successfully\");
  
  // Clear the display buffer
  display.clearDisplay();
 

  display.clearDisplay();


  
 R start typing over things
  
  // Draw a rectangle around the edge
  display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
  
  // Send the buffer to the display
  display.display();
  
  Serial.println("");
}

void loop() {
  // Simple animation to verify display is working
  static unsigned long lastTime = 0;
  static int counter = 0;
  
  if (millis() - lastTime > 1000) {
    lastTime = millis();
    counter++;
    
    // Update just the counter part of the display
    display.fillRect(0, 48, SCREEN_WIDTH, 16, SSD1306_BLACK);
    display.setCursor(0, 48);
    display.print("Counter: ");
    display.println(counter);
    display.display();
  }* :reg.      Shows registers
}
void foo(){
    if (bar) {
        something();
    }
    * :reg.      Shows registers
|   // <- cursor here
    while (baz) {
        {
    if (bar) {
        something();
        R start typing over things
    }
|   // <- cursor here
* :reg.      Shows registers
    while (baz) {
        stuff();
    }
}
        stuff();
    }{
        R start typing over things
    if (bar) {
        something();
    }
|   // <- cursor here
    while (baz) {
        stuff();
    }
}
} 