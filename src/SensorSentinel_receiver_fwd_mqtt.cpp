/**
 * @file SensorSentinel_receiver_fwd_mqtt.cpp
 * @brief Sensor and GNSS packet receiver for Heltec boards with MQTT forwarding
 *
 * Listens for incoming packets, displays them, and forwards them to MQTT topics:
 * - Sensor packets go to lora/sensor
 * - GNSS packets go to lora/gnss
 * Uses the SensorSentinel_mqtt_helper for MQTT connectivity.
 */

#include "heltec_unofficial_revised.h"
#include "SensorSentinel_packet_helper.h"
#include "SensorSentinel_mqtt_helper.h"
#include "SensorSentinel_wifi_helper.h"

#ifndef NO_RADIOLIB
#include "SensorSentinel_RadioLib_helper.h"
#endif

// Global variables
uint8_t packetBuffer[MAX_LORA_PACKET_SIZE]; // Buffer to hold received packet
unsigned long lastPacketTime = 0;
uint32_t packetsReceived = 0;
uint32_t packetsForwarded = 0;

// Variables for packet processing
String packetMessageType; // Current packet message type

// Function prototypes
void onBinaryPacketReceived(uint8_t *data, size_t length, float rssi, float snr);

#define STARTUP_DISPLAY_DELAY 2000

void setup()
{
  // Initialize the Heltec board
  heltec_setup();

  Serial.printf("Sensor packet size: %d bytes\n", sizeof(SensorSentinel_sensor_packet_t));
  Serial.printf("Pin readings size: %d bytes\n", sizeof(SensorSentinel_pin_readings_t));
  Serial.printf("GNSS packet size: %d bytes\n", sizeof(SensorSentinel_gnss_packet_t));

  heltec_clear_display();
  both.println("Packet Receiver+MQTT");
  both.printf("Board: %s\n", heltec_get_board_name());
  both.printf("Battery: %d%% (%.2fV)\n",
              heltec_battery_percent(),
              heltec_vbat());

// Subscribe to binary packet reception
#ifndef NO_RADIOLIB
  if (SensorSentinel_subscribe(NULL, onBinaryPacketReceived))
  {
    both.println("\nSubscribed to packets");
  }
  else
  {
    both.println("\nSubscribe failed!");
  }
#else
  both.println("\nNo radio, No sub");
#endif

  heltec_display_update();
  delay(STARTUP_DISPLAY_DELAY);

  // Let the helper libraries handle WiFi and MQTT connection
  SensorSentinel_wifi_begin();
  SensorSentinel_mqtt_setup(true); // With time sync
}

void loop()
{
  // Handle system tasks
  heltec_loop();

#ifndef NO_RADIOLIB
  // Process any pending packet receptions
  SensorSentinel_process_packets();
#endif

  // Let helpers maintain WiFi and MQTT connections
  SensorSentinel_wifi_maintain();
  SensorSentinel_mqtt_maintain();
}

/**
 * Callback for when a binary packet is received
 */

void onBinaryPacketReceived(uint8_t *data, size_t length, float rssi, float snr)
{
  // Turn on LED to indicate reception
  heltec_led(25);

  // Clear display for output
  heltec_clear_display();

   // Store packet in buffer
  memcpy(packetBuffer, data, length);

  // Basic validation - check if it's a known message type with the right size
  bool isValidPacket = SensorSentinel_validate_packet(packetBuffer, length);

  if (isValidPacket)
  {
    uint8_t messageType = *((uint8_t *)data);
    packetMessageType = SensorSentinel_message_type_to_string(messageType);

    // Display the extracted information
    both.printf("\nReceived Type: %s\n", packetMessageType.c_str());
    both.printf("Msg #: %u\n", SensorSentinel_get_message_counter(packetBuffer));
    both.printf("NodeID: %u\n", SensorSentinel_extract_node_id_from_packet(packetBuffer));

    // Common display elements (outside the if/else block)
    both.printf("RSSI: %.1f dB,\nSNR: %.1f dB\n", rssi, snr);
    both.printf("Size: %u bytes\n", length);
    both.printf("Total Rx: %u\n", packetsReceived + 1);

    // Serial output for valid packets
    SensorSentinel_print_packet_info(packetBuffer, length);
    Serial.println("---------------------------");

    // Forward the packet to MQTT using new helper function
    MqttForwardStatus mqttStatus = SensorSentinel_mqtt_forward_packet(packetBuffer, length, rssi, snr);
    
    // Display MQTT status
    both.printf("MQTT: %s\n", SensorSentinel_mqtt_status_to_string(mqttStatus));
    
    if (mqttStatus == MQTT_SUCCESS) {
        packetsForwarded++;
        // Serial output for packet statistics
        Serial.printf("\nPackets received: %u, Forwarded: %u\n", packetsReceived + 1, packetsForwarded);
        Serial.println("---------------------------");
        Serial.println("---------------------------\n\n");
    }
  } 
  else 
  {
    // Handle invalid packet case
    both.println("Invalid packet received");
    SensorSentinel_print_invalid_packet(data, length);
  }

  heltec_display_update();

  // Update counters and timers
  packetsReceived++;
  lastPacketTime = millis();

  // Turn off LED
  heltec_led(0);
}