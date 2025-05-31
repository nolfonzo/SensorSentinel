/**
 * Heltec LoRa to MQTT Gateway
 * - Receives LoRa packets and forwards to MQTT
 * - Based on the same style as the original bidirectional code
 */

// Enable power button functionality
#define HELTEC_POWER_BUTTON

// Include libraries
#include "heltec_unofficial.h"
#include "driver/gpio.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// WiFi and MQTT Configuration
#define WIFI_SSID       "Nuevo Extremo"
#define WIFI_PASSWORD   "nolfonzo"
#define MQTT_SERVER     "192.168.20.101"
#define MQTT_PORT       1883
#define MQTT_TOPIC      "lora/sensor"     // Base topic
#define MQTT_STATUS     "lora/gateway"    // Gateway status topic

// Global variables
String rxdata;
volatile bool rxFlag = false;        // Set by interrupt when packet received
uint32_t packetCounter = 0;          // Counter for received packets
unsigned long lastStatusTime = 0;    // For periodic status updates
const unsigned long statusInterval = 30000; // Status update every 30 seconds

// MQTT and WiFi clients
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// Interrupt callback for packet reception
void rx() {
  rxFlag = true;
}

// Connect to WiFi
void setupWiFi() {
  heltec_clear_display(1, 1);
  both.println("Connecting to WiFi");
  both.println(WIFI_SSID);
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    both.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    both.println("\nConnected!");
    both.printf("IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    both.println("\nConnection failed");
  }
}

// Connect to MQTT broker
boolean connectMQTT() {
  heltec_clear_display(1, 1);
  both.println("Connecting to MQTT");
  both.println(MQTT_SERVER);
  
  // Create a random client ID
  String clientId = "HeltecGW-";
  clientId += String(random(0xffff), HEX);
  
  // Connect to MQTT broker
  boolean result = mqttClient.connect(clientId.c_str());
  
  if (result) {
    both.println("Connected!");
    
    // Publish a connection message
    JsonDocument doc;
    doc["status"] = "online";
    doc["gateway_id"] = clientId;
    doc["board"] = heltec_get_board_name();
    
    char buffer[128];
    serializeJson(doc, buffer);
    mqttClient.publish(MQTT_STATUS, buffer, true); // retained message
  } else {
    both.printf("Failed, rc=%d\n", mqttClient.state());
  }
  
  return result;
}
// Process received LoRa packet and publish to MQTT
void handleReceivedPacket() {
  rxFlag = false; // Clear the flag
  
  #ifndef HELTEC_NO_RADIO_INSTANCE
    // Read the received data
    int state = radio.readData(rxdata);
    
    if (state == RADIOLIB_ERR_NONE) {
      packetCounter++;

      Serial.println("\n\n==================================================");
      Serial.println("                NEW PACKET RECEIVED                ");
      Serial.println("=================================================="); 


      // Display received data on the board screen
      heltec_clear_display(1, 1);
      both.printf("Packet #%d\n", packetCounter);
      both.printf("RSSI: %.1f dBm\n", radio.getRSSI());
      both.printf("SNR: %.1f dB\n", radio.getSNR());
      both.printf("RX: %s\n", rxdata.c_str());
      Serial.printf("Packet Length: %d bytes\n", rxdata.length());
      Serial.println("\nReceived LoRa Payload:");
      Serial.println(rxdata);
      
      // Brief LED flash for received packet
      heltec_led(25);
      delay(100);
      heltec_led(0);
      
      // Create JSON payload for MQTT
      JsonDocument doc;
      
      doc["gateway"] = WiFi.macAddress();
      doc["rssi"] = radio.getRSSI();
      doc["snr"] = radio.getSNR();
      doc["packet_id"] = packetCounter;
      doc["length"] = rxdata.length();
      
      // Parse the received data (format: "#5\nBat:95%\nTemp:24.2C\nup:120s")
      if (rxdata.indexOf("#") == 0 && rxdata.indexOf("\n") > 0) {
        // Extract message counter
        int firstNewline = rxdata.indexOf("\n");
        String counterStr = rxdata.substring(1, firstNewline);
        doc["node_counter"] = counterStr.toInt();
        
        // Try to extract battery level
        if (rxdata.indexOf("Bat:") >= 0) {
          int batStart = rxdata.indexOf("Bat:") + 4;
          int batEnd = rxdata.indexOf("%", batStart);
          if (batEnd > batStart) {
            String batStr = rxdata.substring(batStart, batEnd);
            doc["battery"] = batStr.toInt();
          }
        }
        
        // Try to extract temperature
        if (rxdata.indexOf("Temp:") >= 0) {
          int tempStart = rxdata.indexOf("Temp:") + 5;
          int tempEnd = rxdata.indexOf("C", tempStart);
          if (tempEnd > tempStart) {
            String tempStr = rxdata.substring(tempStart, tempEnd);
            doc["temperature"] = tempStr.toFloat();
          }
        }
        
        // Try to extract uptime - now labeled "up" instead of "Uptime"
        if (rxdata.indexOf("up:") >= 0) {
          int timeStart = rxdata.indexOf("up:") + 3;
          int timeEnd = rxdata.indexOf("s", timeStart);
          if (timeEnd > timeStart) {
            String timeStr = rxdata.substring(timeStart, timeEnd);
            doc["uptime_sec"] = timeStr.toInt();
          }
        }
      } else {
        // If not in expected format, just include the raw data
        doc["data"] = rxdata;
      }
      
      // Publish to MQTT if connected
      if (mqttClient.connected()) {
        char buffer[256];
        serializeJson(doc, buffer);
        mqttClient.publish(MQTT_TOPIC, buffer);
        both.println("\nPublished to MQTT");
        
        // Print the MQTT payload and actual received content
        Serial.println("MQTT Payload:");
        serializeJsonPretty(doc, Serial);
        Serial.println();
      } else {
        both.println("MQTT disconnected");
      }
    } else {
      heltec_clear_display(1, 1);
      both.printf("RX Error: %i\n", state);
      Serial.printf("RadioLib RX Error: %i\n", state);
    }
    
    // Continue listening
    radio.setDio1Action(rx);
    RADIOLIB_OR_HALT(radio.startReceive());
  #endif
}

void setup() {
  // Initialize serial for debug output
  Serial.begin(115200);
  
  // Initialize Heltec library
  heltec_setup();
  
  // Install GPIO ISR service (critical for interrupts to work)
  gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1);
  
  // Display board information
  heltec_clear_display(1, 1);
  both.println("LoRa MQTT Gateway");
  both.printf("Board: %s\n", heltec_get_board_name());
  
  // Setup WiFi
  setupWiFi();
  
  // Configure MQTT
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  
  // Connect to MQTT if WiFi connected
  if (WiFi.status() == WL_CONNECTED) {
    connectMQTT();
  }
  
  #ifndef HELTEC_NO_RADIO_INSTANCE
    // Initialize radio
    int state = radio.begin();
    if (state != RADIOLIB_ERR_NONE) {
      both.printf("Radio init failed: %d\n", state);
      while (true) { delay(1000); }  // Hang if radio fails
    }
    
    // Set interrupt callback for received packets
    radio.setDio1Action(rx);
    
    // Start listening for packets
    both.println("Starting to listen...");
    Serial.println("Starting to listen for LoRa packets...");
    RADIOLIB_OR_HALT(radio.startReceive());
  #else
    both.println("No radio available");
  #endif
  
  both.println("Ready!");
  Serial.println("\n====== LoRa MQTT Gateway Ready ======");
  Serial.printf("WiFi connected to: %s\n", WIFI_SSID);
  Serial.printf("MQTT server: %s\n", MQTT_SERVER);
  Serial.printf("Gateway IP: %s\n", WiFi.localIP().toString().c_str());
  Serial.println("=====================================\n");
}

void loop() {
  // Handle button, power control, and display updates
  heltec_loop();
  
  // Maintain MQTT connection
  if (WiFi.status() == WL_CONNECTED) {
    if (!mqttClient.connected()) {
      // Try to reconnect every 5 seconds
      static unsigned long lastReconnectAttempt = 0;
      unsigned long now = millis();
      if (now - lastReconnectAttempt > 5000) {
        lastReconnectAttempt = now;
        if (connectMQTT()) {
          lastReconnectAttempt = 0;
        }
      }
    } else {
      // Client connected - process MQTT messages
      mqttClient.loop();
    }
  } else {
    // Try to reconnect to WiFi if disconnected
    static unsigned long lastWifiAttempt = 0;
    unsigned long now = millis();
    if (now - lastWifiAttempt > 30000) {
      lastWifiAttempt = now;
      setupWiFi();
      if (WiFi.status() == WL_CONNECTED) {
        connectMQTT();
      }
    }
  }
  
  // Handle received packet
  if (rxFlag) {
    handleReceivedPacket();
  }
  
  // Publish gateway status every 30 seconds
  if (millis() - lastStatusTime > statusInterval) {
    lastStatusTime = millis();
    Serial.println("\n====================================="); 
    if (mqttClient.connected()) {
      JsonDocument statusDoc;
      statusDoc["status"] = "online";
      statusDoc["uptime_sec"] = millis() / 1000;
      statusDoc["received_packets"] = packetCounter;
      statusDoc["free_heap"] = ESP.getFreeHeap();
      statusDoc["rssi"] = WiFi.RSSI();
      
      char buffer[256];
      serializeJson(statusDoc, buffer);
      mqttClient.publish(MQTT_STATUS, buffer, true); // retained message
    }  

    // Also update the display
    heltec_clear_display(1, 1);
    both.println("MQTT Gateway Status");
    both.printf("Packets: %d\n", packetCounter);
    both.printf("WiFi: %s\n", WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
    both.printf("MQTT: %s\n", mqttClient.connected() ? "Connected" : "Disconnected");
    both.printf("Uptime: %d min\n", millis() / 60000);
    Serial.println("=====================================\n"); 
  }
  delay(20);
}