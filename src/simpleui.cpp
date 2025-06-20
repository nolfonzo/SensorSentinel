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
  
  Serial.println("OLED initialized successfully");
  
  // Clear the display buffer
  display.clearDisplay();
  
  // Set text properties
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  // Add content to the display
  display.setCursor(0, 0);
  display.println("Heltec LoRa 32 v3.2");
  display.setCursor(0, 16);
  display.println("OLED Test");
  display.setCursor(0, 32);
  display.println("Adafruit Library");
  
  // Draw a rectangle around the edge
  display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
  
  // Send the buffer to the display
  display.display();
  
  Serial.println("Display content set");
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
  }
}