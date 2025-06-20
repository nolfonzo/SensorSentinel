/**
 * @file SensorSentinel_packet_receiver_fwd_mqtt.cpp
 * @brief Sensor and GNSS packet receiver for Heltec boards with MQTT forwarding
 *
 * Listens for incoming packets, displays them, and forwards them to MQTT topics:
 * - Sensor packets go to lora/sensor
 * - GNSS packets go to lora/gnss
 * Uses the SensorSentinel_mqtt_gateway for MQTT connectivity.
 */

#include "heltec_unofficial_revised.h"
#include "SensorSentinel_packet_helper.h"
#include "SensorSentinel_mqtt_gateway.h"
#include "SensorSentinel_wifi_helper.h"
#include "SensorSentinel_RadioLib_helper.h"

// Configuration
#define MAX_LORA_PACKET_SIZE 256 // Maximum packet size we can handle

// Global variables
uint8_t packetBuffer[MAX_LORA_PACKET_SIZE]; // Buffer to hold received packet
unsigned long lastPacketTime = 0;
uint32_t packetsReceived = 0;
uint32_t packetsForwarded = 0;

// Variables for packet processing
String packetMessageType; // Current packet message type

// Function prototypes
void onBinaryPacketReceived(uint8_t *data, size_t length, float rssi, float snr);
bool forwardPacketToMQTT(uint8_t *data, size_t length, float rssi, float snr);

void setup()
{
  // Initialize the Heltec board
  heltec_setup();

  // Clear display and show startup message
  heltec_clear_display();
  both.println("Packet Receiver+MQTT");
  both.printf("Board: %s\n", heltec_get_board_name());
  both.printf("Battery: %d%% (%.2fV)\n",
              heltec_battery_percent(),
              heltec_vbat());

  // Subscribe to binary packet reception
  if (SensorSentinel_subscribe_binary_packets(onBinaryPacketReceived))
  {
    both.println("Subscribed to packets");
  }
  else
  {
    both.println("Subscribe failed!");
  }
  heltec_display_update();
  delay(2000);

  // Let the helper libraries handle WiFi and MQTT connection
  SensorSentinel_wifi_begin();
  SensorSentinel_mqtt_setup(true); // With time sync
}

void loop()
{
  // Handle system tasks
  heltec_loop();

  // Process any pending packet receptions
  SensorSentinel_process_packets();

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
  Serial.println("\nPacket Received!");

  // Check packet size
  if (length > MAX_LORA_PACKET_SIZE)
  {
    Serial.printf("ERROR: Packet too large: %u bytes\n", length);
    heltec_clear_display();
    both.println("Packet too large!");
    both.printf("Size: %u bytes (max %u)\n", length, MAX_LORA_PACKET_SIZE);
    both.printf("RSSI: %.1f dB, SNR: %.1f dB\n", rssi, snr);
    heltec_display_update();
    delay(2000);
    heltec_led(0);
    return;
  }

  // Store packet in buffer
  memcpy(packetBuffer, data, length);

  // Basic validation - check if it's a known message type with the right size
  bool isValidPacket = SensorSentinel_validate_packet(packetBuffer, length, true);

  // Calculate time since last packet (for Serial output only)
  if (lastPacketTime > 0)
  {
    unsigned long timeBetweenPackets = (millis() - lastPacketTime) / 1000;
    Serial.printf("Time since last packet: ");
    if (timeBetweenPackets < 60)
    {
      Serial.printf("%lu seconds\n", timeBetweenPackets);
    }
    else if (timeBetweenPackets < 3600)
    {
      Serial.printf("%lu minutes\n", timeBetweenPackets / 60);
    }
    else
    {
      Serial.printf("%lu hours\n", timeBetweenPackets / 3600);
    }
  }

  if (isValidPacket)
  {

    // Display the extracted information
    both.printf("Received Type: %s\n", packetMessageType.c_str());
    both.printf("Msg #: %u\n", messageCounter);
    both.printf("NodeID: %u\n", packetJson["nodeId"].as<uint32_t>());

    // Print the raw data for debugging
    Serial.printf("Packet data");
    for (size_t i = 0; i < length; i++)
    {
      Serial.printf("%02X", packetBuffer[i]);
    }
    Serial.println();

    // Serial output for valid packets
    SensorSentinel_print_packet_info(packetBuffer, true);

    Serial.printf("Raw data (%u bytes): ", length);
    for (size_t i = 0; i < length; i++)
    {
      Serial.printf("%02X", packetBuffer[i]);
    }
    Serial.println();

    // Common display elements (outside the if/else block)
    both.printf("RSSI: %.1f dB,\nSNR: %.1f dB\n", rssi, snr);
    both.printf("Size: %u bytes\n", length);
    both.printf("Total Rx: %u\n", packetsReceived + 1);
    Serial.println("---------------------------");

    // Forward the packet to MQTT - global variables are used internally
    bool mqttForwarded = forwardPacketToMQTT(packetBuffer, length, rssi, snr);

    // Serial output for packet statistics
    Serial.printf("Packets received: %u, Forwarded: %u\n", packetsReceived + 1, packetsForwarded);
    Serial.println("---------------------------");
    Serial.println("---------------------------\n");

    // MQTT status display
    if (!mqttForwarded)
    {
      both.println("MQTT: Forward failed");
    }
  }

  heltec_display_update();

  // Update counters and timers
  packetsReceived++;
  lastPacketTime = millis();

  // Turn off LED
  heltec_led(0);
}

/**
 * Forward a received packet to the appropriate MQTT topic
 * @param data Pointer to the packet data
 * @param length Length of the packet in bytes
 * @param rssi RSSI value for the received packet
 * @param snr SNR value for the received packet
 * @return true if forwarding was successful, false otherwise
 */
bool forwardPacketToMQTT(uint8_t *data, size_t length, float rssi, float snr)
{
  // Skip if MQTT is not connected
  if (!SensorSentinel_mqtt_get_client().connected())
  {
    Serial.println("MQTT not connected - cannot forward packet");
    return false;
  }

  if (isValidPacket)
  {

    const char *topic;

    if (msgType == "sensor")
    {
      topic = MQTT_TOPIC_SENSOR;
    }
    else if (msgType == "gnss")
    {
      topic = MQTT_TOPIC_GNSS;
    }
    else
    {
      topic = MQTT_TOPIC_DATA;
    }
    bool success = SensorSentinel_mqtt_get_client().publish(topic, data, length, false);

    if (success)
    {
      packetsForwarded++;
      Serial.printf("Forwarded raw data (%u bytes) to %s\n", length, topic);
      return true;
    }
    else
    {
      Serial.printf("ERROR: Failed to forward raw data to MQTT topic: %s\n", topic);
      return false;
    }
  }
}