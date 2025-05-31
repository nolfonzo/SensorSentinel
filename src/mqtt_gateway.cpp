/**
 * Heltec LoRa MQTT Gateway - Simplified
 */

#define HELTEC_POWER_BUTTON  // Enable power button functionality
#include "heltec_unofficial.h"

// MQTT Configuration
#define MQTT_ENABLED       true
#define WIFI_SSID          "Nuevo Extremo"
#define WIFI_PASSWORD      "nolfonzo" 
#define MQTT_SERVER        "192.168.20.101"
#define MQTT_PORT          1883
#define MQTT_BASE_TOPIC    "lora/gateway/"

#if MQTT_ENABLED
  #include <WiFi.h>
  #include <PubSubClient.h>
  WiFiClient wifiClient;
  PubSubClient mqttClient(wifiClient);
  bool mqttConnected = false;
#endif

// Global variables
volatile bool packetReceived = false;
uint32_t packetCounter = 0;
uint8_t packetBuffer[256];
size_t packetLength = 0;
float lastRssi = 0;
float lastSnr = 0;

// Interrupt handler for packet reception
void onReceive() {
  Serial.println("DI01 INTERRUPT TRIGGERED");
  packetReceived = true;
}

// Connect to WiFi and MQTT
void setupConnectivity() {
  #if MQTT_ENABLED
    both.println("Connecting to WiFi");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int timeout = 0;
    while (WiFi.status() != WL_CONNECTED && timeout < 20) {
      delay(500);
      both.print(".");
      timeout++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      both.println("\nWiFi connected!");
      both.printf("IP: %s\n", WiFi.localIP().toString().c_str());
      
      // Configure MQTT connection
      mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
      
      // Create a random client ID
      String clientId = "HeltecGW-";
      clientId += String(random(0xffff), HEX);
      
      // Connect to MQTT
      if (mqttClient.connect(clientId.c_str())) {
        both.println("MQTT connected!");
        mqttClient.publish(MQTT_BASE_TOPIC "status", "Gateway online");
        mqttConnected = true;
      } else {
        both.println("MQTT connection failed");
      }
    } else {
      both.println("\nWiFi connection failed");
    }
  #endif
}

// Publish packet to MQTT
void publishToMqtt(const heltec_status_packet_t* packet) {
  #if MQTT_ENABLED
    if (!mqttConnected || !WiFi.isConnected()) return;
    
    char jsonBuffer[512];
    // Format JSON with signal metrics
    snprintf(jsonBuffer, sizeof(jsonBuffer),
      "{"
      "\"message_type\":%u,"
      "\"board_id\":\"0x%X\","
      "\"counter\":%u,"
      "\"battery\":%u,"
      "\"analog\":[%u,%u,%u,%u],"
      "\"digital\":%u,"
      "\"rssi\":%.1f,"
      "\"snr\":%.1f"
      "}",
      packet->messageType,
      packet->boardId,
      packet->messageCounter,
      packet->batteryPercent,
      packet->analog[0], packet->analog[1], packet->analog[2], packet->analog[3],
      packet->boolean,
      lastRssi,
      lastSnr
    );
    
    // Publish to data topic
    mqttClient.publish(MQTT_BASE_TOPIC "data", jsonBuffer);
    both.println("Published to MQTT");
  #endif
}

void setup() {
  // Initialize library
  heltec_setup();
  
  // Welcome message
  heltec_clear_display(1, 1);
  both.println("LoRa MQTT Gateway");
  both.printf("Board: %s\n", heltec_get_board_name());
  
  // Setup connectivity
  setupConnectivity();
  
  // Configure radio for reception
  radio.setDio1Action(onReceive);
  radio.startReceive();
  
  both.println("Listening...");
}

void loop() {
  // Handle system tasks
  heltec_loop();
  
  // Handle MQTT client
  #if MQTT_ENABLED
    if (mqttConnected) {
      mqttClient.loop();
    }
  #endif

// Process received packet
if (packetReceived) {
  packetReceived = false;
  
  // Get the actual packet length from the radio
  int packetLength = radio.getPacketLength();
  
  // Create a buffer exactly the right size (or use a max-sized buffer)
  uint8_t buffer[255];  // Max LoRa packet size
  
  // Read exactly that many bytes
  int state = radio.readData(buffer, packetLength);
  
  if (state == RADIOLIB_ERR_NONE) {
    // Process the packet based on message type
    uint8_t messageType = buffer[0];
    
    if (messageType == 0x01 || messageType == 0x02) {
      // It's a status packet
      heltec_status_packet_t packet;
      memset(&packet, 0, sizeof(packet));  // Clear structure first
      // Cast sizeof to int (safe since packet won't be larger than INT_MAX)
      memcpy(&packet, buffer, std::min(packetLength, (int)sizeof(packet)));
        
      // Process packet...
      both.printf("Type: 0x%02X\n", packet.messageType);
      both.printf("ID: 0x%X\n", packet.boardId);
      both.printf("Count: %u\n", packet.messageCounter);
    }
  }
  
  // Start receiving again
  radio.startReceive();
} 
  // Show status every 30 seconds
  static uint32_t lastStatusTime = 0;
  if (millis() - lastStatusTime > 30000) {
    lastStatusTime = millis();
    
    heltec_clear_display(1, 1);
    both.printf("Packets: %u\n", packetCounter);
    both.printf("Uptime: %u min\n", millis()/60000);
    
    #if MQTT_ENABLED
      if (WiFi.isConnected()) {
        both.println("WiFi: Connected");
        
        if (!mqttClient.connected()) {
          both.println("Reconnecting MQTT...");
          String clientId = "HeltecGW-";
          clientId += String(random(0xffff), HEX);
          
          if (mqttClient.connect(clientId.c_str())) {
            mqttConnected = true;
            both.println("MQTT: Connected");
            mqttClient.publish(MQTT_BASE_TOPIC "status", "Gateway online");
          } else {
            mqttConnected = false;
            both.println("MQTT: Failed");
          }
        } else {
          both.println("MQTT: Connected");
        }
      } else {
        both.println("WiFi: Disconnected");
        WiFi.reconnect();
      }
    #endif
    
    both.println("Listening...");
  }
}