/**
 * Universal Heltec LoRa Bidirectional Transceiver
 * - Using the library's packet subscription system
 */

// Enable power button functionality
#define HELTEC_POWER_BUTTON

// Include the Heltec library
#include "heltec_unofficial.h"

// Configuration
#define PAUSE               10          // Seconds between auto-transmissions (0 = manual only)

// Global variables
long messageCounter = 0;
uint64_t lastTxTime = 0;
uint64_t txDuration;
uint64_t minimumPause;                 // For 1% duty cycle compliance
bool pendingGpsTransmit = false;
bool gpsInitialized = false;  
String lastTxMessage = "";

// Packet receive callback function
void onPacketReceived(String &data, float rssi, float snr) {
  // It's a received packet from another device
  heltec_clear_display(2, 1);
  both.printf("RX %s", data.c_str());
  both.printf("\nRSSI: %.1f dBm", rssi);
  both.printf("\nSNR: %.1f dB", snr);
  both.println("\n---");
  
  // Brief LED flash for received packet
  heltec_led(25);
  delay(100);
  heltec_led(0);
}

void initializeGPS() {
  heltec_clear_display(2, 1);
  
  #ifdef HELTEC_GNSS
    if (!gpsInitialized) {
      both.println("Initializing GPS...");
      heltec_gnss_begin();
      gpsInitialized = true;
      both.println("GPS initialized");
      delay(1000); // Give GPS a moment to start
    }
  #else
    both.println("GPS not available");
    both.println("on this board");
    delay(2000);
  #endif
}

void transmitPacket(bool sendGPS = false) {
  // First, prepare all our data before unsubscribing
  String message;
  
  #ifdef HELTEC_GNSS
    if (sendGPS && gpsInitialized) {
      // Create GPS location message
      message = "GPS Loc";
      message += "\nLat:" + String(gps.location.lat(), 6);
      message += "\nLon:" + String(gps.location.lng(), 6);
      message += "\nAlt:" + String(gps.altitude.meters(), 1) + "m";
      message += "\nSats:" + String(gps.satellites.value());
    } else {
      sendGPS = false; // Fall back if GPS not initialized
    }
  #else
    sendGPS = false; // No GPS available on this board
  #endif

  if (!sendGPS) { 
    // Create regular status message
    message = "#" + String(messageCounter++);
    message += "\nBat:" + String(heltec_battery_percent()) + "%";
    message += "\nTemp:" + String(heltec_temperature(), 1) + "C";
    message += "\nUp:" + String(millis()/1000) + "s";
  }
  
  // Clear the display and show TX message BEFORE unsubscribing
  heltec_clear_display(2, 1);
  both.printf("TX %s ", message.c_str());
  
  #ifndef HELTEC_NO_RADIO_INSTANCE
    // Now unsubscribe just before transmission
    heltec_unsubscribe_packets();
    
    // LED on during transmission
    heltec_led(50);
    
    // Measure transmission time for duty cycle calculation
    txDuration = millis();
    int state = radio.transmit(message);
    txDuration = millis() - txDuration;
    
    // Immediately resubscribe after transmission
    heltec_subscribe_packets(onPacketReceived);
    
    // LED off
    heltec_led(0);
    
    if (state == RADIOLIB_ERR_NONE) {
      both.printf("\nOK (%i ms)\n", (int)txDuration);
    } else {
      both.printf("\nFAIL (%i)\n", state);
    }
    
    // Calculate minimum pause for 1% duty cycle (transmission time * 100)
    minimumPause = txDuration * 100;
    lastTxTime = millis();
  #else
    both.println("\nRadio not available");
  #endif
}

void setup() {
  // Initialize serial for debug output
  Serial.begin(115200);
  
  // Initialize Heltec library (this also initializes the radio)
  heltec_setup();
  
  // Display board information
  heltec_clear_display(1, 1);
  both.println("Heltec LoRa Transceiver");
  
  heltec_clear_display(1, 1);
  both.println("Initializing...");
  
  // Subscribe to packet events using the library's subscription method
  heltec_subscribe_packets(onPacketReceived);
  
  // Start listening for packets (done by heltec_subscribe_packets)
  both.println("Listening for packets...");
  
  if (PAUSE > 0) {
    both.printf("Auto-tx every %i sec\n", PAUSE);
  } else {
    both.println("Manual tx (press button)");
  }
  
  #ifdef HELTEC_GNSS
    both.println("Button = GPS location");
  #else
    both.println("Button = Manual TX");
  #endif
  
  both.println("Ready!");
  both.println("---");
}

void loop() {
  // Handle button, power control, GNSS updates, and display updates
  // This also calls heltec_process_packets() which processes LoRa packets
  heltec_loop();
  
  bool txLegal = millis() > lastTxTime + minimumPause;
  bool timeToTransmit = (PAUSE > 0) && (millis() - lastTxTime > (PAUSE * 1000));
  bool buttonPressed = heltec_button_clicked();

  #ifdef HELTEC_GNSS
    // Update GNSS data continuously if initialized
    if (gpsInitialized) {
      heltec_gnss_update();
    }
    
    if (buttonPressed) { // if button is pressed, we'll send GPS info
      initializeGPS();
      
      // Check GPS status after initialization
      if (gpsInitialized) {
        int sats = gps.satellites.value();
        
        if (sats == 0) {
          // No satellites - just show message and don't queue transmission
          heltec_clear_display(2,1);
          both.println("GPS Status");
          both.println("No satellites");
          both.println("acquired");
          delay(2000);
          return; // Exit early, don't set pendingGpsTransmit
        } else {
          // Has satellites - queue the transmission
          pendingGpsTransmit = true;
          heltec_clear_display(2, 1);
          both.printf("GPS ready\n%i satellites\nQueued for TX", sats);
          delay(1000);
        }
      }
    }
  #else
    // For non-GNSS boards, button press triggers normal transmission
    if (buttonPressed && txLegal) {
      // Display clearing happens inside transmitPacket
      transmitPacket(false);
    } else if (buttonPressed && !txLegal) {
      int waitSeconds = ((minimumPause - (millis() - lastTxTime)) / 1000) + 1;
      heltec_clear_display(2,1);
      both.printf("Tx queued\nDuty cycle\nWait %i secs", waitSeconds);
    }
  #endif

  // Handle pending GPS transmission
  if (pendingGpsTransmit && txLegal) {
    #ifdef HELTEC_GNSS
      // Display clearing happens inside transmitPacket
      transmitPacket(true); 
    #endif
    pendingGpsTransmit = false;
  } else if (pendingGpsTransmit && !txLegal) {
    int waitSeconds = ((minimumPause - (millis() - lastTxTime)) / 1000) + 1;
    heltec_clear_display(2,1);
    both.printf("GPS Tx queued\nDuty cycle\nWait %i secs", waitSeconds);
  }
  
  // Handle automatic transmission based on timer
  if (timeToTransmit && txLegal) {
    // Display clearing happens inside transmitPacket
    transmitPacket(false);
  }
}