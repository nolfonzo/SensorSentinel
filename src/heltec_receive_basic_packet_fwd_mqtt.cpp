/**
 * Heltec LoRa to MQTT Gateway
 * - Receives LoRa packets and forwards to MQTT
 */

// Enable power button functionality
#define HELTEC_POWER_BUTTON

// Include libraries
#include "heltec_unofficial.h"
#include "heltec_mqtt_gateway.h"  
#include "heltec_wifi_helper.h"

// Global variables
uint32_t packetCounter = 0;          // Counter for received packets
unsigned long lastStatusTime = 0;    // For periodic status updates
const unsigned long statusInterval = 30000; // Status update every 30 seconds

// Packet handler callback
void handlePacket(String &data, float rssi, float snr) {
  packetCounter++;

  Serial.println("\n\n==================================================");
  Serial.println("                NEW PACKET RECEIVED                ");
  Serial.println("=================================================="); 

  // Display received data on the board screen
  heltec_clear_display(1, 1);
  both.printf("Packet #%d\n", packetCounter);
  both.printf("RSSI: %.1f dBm\n", rssi);
  both.printf("SNR: %.1f dB\n", snr);
  both.printf("RX: %s\n", data.c_str());
  Serial.printf("Packet Length: %d bytes\n", data.length());
  Serial.println("\nReceived LoRa Payload:");
  Serial.println(data);
  
  // Brief LED flash for received packet
  heltec_led(25);
  delay(100);
  heltec_led(0);
  
  // Create JSON payload for MQTT
  JsonDocument doc;
  
  doc["gateway"] = heltec_wifi_mac();  // Updated to use heltec_ prefix
  doc["rssi"] = rssi;
  doc["snr"] = snr;
  doc["packet_id"] = packetCounter;
  doc["length"] = data.length();
  
  // Parse the received data (format: "#5\nBat:95%\nTemp:24.2C\nup:120s")
  if (data.indexOf("#") == 0 && data.indexOf("\n") > 0) {
    // Extract message counter
    int firstNewline = data.indexOf("\n");
    String counterStr = data.substring(1, firstNewline);
    doc["node_counter"] = counterStr.toInt();
    
    // Try to extract battery level
    if (data.indexOf("Bat:") >= 0) {
      int batStart = data.indexOf("Bat:") + 4;
      int batEnd = data.indexOf("%", batStart);
      if (batEnd > batStart) {
        String batStr = data.substring(batStart, batEnd);
        doc["battery"] = batStr.toInt();
      }
    }
    
    // Try to extract temperature
    if (data.indexOf("Temp:") >= 0) {
      int tempStart = data.indexOf("Temp:") + 5;
      int tempEnd = data.indexOf("C", tempStart);
      if (tempEnd > tempStart) {
        String tempStr = data.substring(tempStart, tempEnd);
        doc["temperature"] = tempStr.toFloat();
      }
    }
    
    // Try to extract uptime - now labeled "up" instead of "Uptime"
    if (data.indexOf("up:") >= 0) {
      int timeStart = data.indexOf("up:") + 3;
      int timeEnd = data.indexOf("s", timeStart);
      if (timeEnd > timeStart) {
        String timeStr = data.substring(timeStart, timeEnd);
        doc["uptime_sec"] = timeStr.toInt();
      }
    }
  } else {
    // If not in expected format, just include the raw data
    doc["data"] = data;
  }
  
  // Publish to MQTT using the new gateway module
  if (heltec_mqtt_publish_json(MQTT_TOPIC, doc)) {  // Updated to use heltec_ prefix
    both.println("\nPublished to MQTT");
    
    // Print the MQTT payload and actual received content
    Serial.println("MQTT Payload:");
    serializeJsonPretty(doc, Serial);
    Serial.println();
  } else {
    both.println("MQTT disconnected");
  }
}

void setup() {
  // Initialize the Heltec board (this also initializes radio)
  heltec_setup();
  
  // Display board information
  heltec_clear_display(1, 1);
  both.println("LoRa MQTT Gateway");
  both.printf("Board: %s\n", heltec_get_board_name());
  
  // Connect to WiFi - this must be done before MQTT setup
  both.println("Connecting to WiFi...");
  if (heltec_wifi_begin(15)) {  // Try up to 15 connection attempts
    both.printf("WiFi connected: %s\n", heltec_wifi_ip().c_str());
  } else {
    both.println("WiFi connection failed");
    Serial.println("WARNING: WiFi not connected, MQTT will not work!");
  }
  
  // Setup MQTT with time sync
  heltec_mqtt_setup(true);  // Updated to use heltec_ prefix
  
  // Subscribe to LoRa packet reception
  heltec_subscribe_packets(handlePacket);
  
  both.println("Ready!");
  Serial.println("\n====== LoRa MQTT Gateway Ready ======");
  Serial.printf("MQTT server: %s\n", MQTT_SERVER);
  Serial.printf("Gateway IP: %s\n", heltec_wifi_ip().c_str());  // Updated to use heltec_ prefix
  Serial.printf("Gateway MAC: %s\n", heltec_wifi_mac().c_str());
  Serial.printf("WiFi status: %s\n", heltec_wifi_status_string().c_str());
  Serial.println("=====================================\n");
}

void loop() {
  // Handle button, power control, display updates, and packet processing
  heltec_loop();
  
  // Maintain WiFi connection
  heltec_wifi_maintain();
  
  // Maintain MQTT connection using the new module
  heltec_mqtt_maintain();  // Updated to use heltec_ prefix
  
  // Publish gateway status every 30 seconds
  if (millis() - lastStatusTime > statusInterval) {
    lastStatusTime = millis();
    Serial.println("\n====================================="); 
    
    // Use the mqtt_gateway module to publish status
    heltec_mqtt_publish_status(packetCounter);  // Updated to use heltec_ prefix
    
    // Also update the display
    heltec_mqtt_display_status(packetCounter);  // Updated to use heltec_ prefix
    
    // Show current WiFi status
    Serial.printf("WiFi status: %s (RSSI: %d dBm)\n", 
                 heltec_wifi_status_string().c_str(),
                 heltec_wifi_rssi());
    
    Serial.println("=====================================\n"); 
  }
  
  delay(20);
}