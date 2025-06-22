/**
 * Universal Heltec LoRa Bidirectional Transceiver
 * - Using the library's packet subscription system
 */

// Enable power button functionality
#define HELTEC_POWER_BUTTON

// Include the Heltec library
#include "heltec_unofficial_revised.h"
#ifndef NO_RADIOLIB
#include "SensorSentinel_RadioLib_helper.h"
#endif


// Global variables
long messageCounter = 0;
uint64_t lastTxTime = 0;
uint64_t txDuration;
uint64_t minimumPause = 10000; // ms, duty cycle
bool gpsSuccess = false;

// Packet receive callback function
void onPacketReceived(String &data, float rssi, float snr)
{
  // It's a received packet from another device
  heltec_clear_display(2, 1);
  both.printf("RX %s", data.c_str());
  both.printf("\nRSSI: %.1f dBm", rssi);
  both.printf("\nSNR: %.1f dB", snr);
  both.println("\n---");

  // Brief LED flash for received packet
  heltec_led(25);
  delay(200);
  heltec_led(0);
}

void transmitPacket(bool sendGPS = false)
{
  // First, prepare all our data before unsubscribing
  String message;

  heltec_clear_display(2, 1);

  if (sendGPS)
  {
#ifdef SensorSentinel_GNSS
    gpsSuccess = SensorSentinel_gnss_update();
    if (gpsSuccess)
    {
      message = "GPS Loc";
      message += "\nLat:" + String(gps.location.lat(), 6);
      message += "\nLon:" + String(gps.location.lng(), 6);
      message += "\nAlt:" + String(gps.altitude.meters(), 1) + "m";
      message += "\nSats:" + String(gps.satellites.value());
    }
    else
    {
      both.println("GPS update error");
      return;
    }
#else
    both.println("GPS requested but no board support");
    return;
#endif
  }
  if (!sendGPS)
  {
    // Create regular status message
    message = "#" + String(messageCounter++);
    message += "\nBat:" + String(heltec_battery_percent()) + "%";
    message += "\nTemp:" + String(heltec_temperature(), 1) + "C";
    message += "\nUp:" + String(millis() / 1000) + "s";
  }

  // Clear the display and show TX message BEFORE unsubscribing
  both.printf("TX %s ", message.c_str());

#ifndef NO_RADIOLIB
  // Now unsubscribe just before transmission
  SensorSentinel_unsubscribe_packets();

  // LED on during transmission
  heltec_led(50);

  // Measure transmission time for duty cycle calculation
  txDuration = millis();
  int state = radio.transmit(message);
  txDuration = millis() - txDuration;

  // Immediately resubscribe after transmission
  SensorSentinel_subscribe_packets(onPacketReceived);

  // LED off
  heltec_led(0);

  if (state == RADIOLIB_ERR_NONE)
  {
    both.printf("\nOK (%i ms)\n", (int)txDuration);
  }
  else
  {
    both.printf("\nFAIL (%i)\n", state);
  }

  // Calculate minimum pause for 1% duty cycle (transmission time * 100)
  minimumPause = txDuration * 10000;
  lastTxTime = millis();
#else
  both.println("\nPacket ready");
  both.println("\nRadio not avail");
  lastTxTime = millis();

#endif
}

void setup()
{
  // Initialize Heltec library (this also initializes the radio)
  heltec_setup();

  // Display board information
  heltec_clear_display(1, 1);
  both.println("LoRa Transceiver");
  both.println("Initializing...");
  delay (1000);

#ifndef NO_RADIOLIB
  // Subscribe to packet events using the library's subscription method
  SensorSentinel_subscribe_packets(onPacketReceived);
  both.println("Listening for packets...");
#else
  both.println("RadioLib not avail");
  both.println("Not subscribed to pkts");
#endif

  both.printf("Auto-tx every %i sec\n", minimumPause / 1000);

#ifdef SensorSentinel_GNSS
  both.println("Button = GPS location");
#else
  both.println("Button = Manual TX");
#endif

  both.println("Ready!");
  both.println("---");
}

void loop()
{
  // Handle button, power control, GNSS updates, and display updates
  // This also calls SensorSentinel_process_packets() which processes LoRa packets
  heltec_loop();
  
  SensorSentinel_process_packets();

  bool txLegal = millis() > lastTxTime + minimumPause;
  bool buttonPressed = heltec_button_clicked();

  if (buttonPressed)
  {
#ifdef SensorSentinel_GNSS
    transmitPacket(true);
#else
    transmitPacket(false);
#endif
  }
  else if (txLegal)
  {
    transmitPacket(false);
  }
}