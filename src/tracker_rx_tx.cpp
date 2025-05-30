/**
 * Universal Heltec LoRa Bidirectional Transceiver
 */

// Enable power button functionality (long press = sleep)
#define HELTEC_POWER_BUTTON

// Include the Heltec library
#include "heltec_unofficial.h"

// Configuration
#define PAUSE               10          // Seconds between auto-transmissions (0 = manual only)
#define FREQUENCY           915.0       // MHz (use 868.0 for Europe)
#define BANDWIDTH           125.0       // kHz
#define SPREADING_FACTOR    9           // 6-12 (higher = longer range, slower)
#define TRANSMIT_POWER      14          // dBm (0-22, but check local regulations)

// Global variables
String rxdata;
volatile bool rxFlag = false;          // Set by interrupt when packet received
long messageCounter = 0;
uint64_t lastTxTime = 0;
uint64_t txDuration;
uint64_t minimumPause;                 // For 1% duty cycle compliance
bool pendingGpsTransmit = false;
bool gpsInitialized = false;  

// Interrupt callback for packet reception
void rx() {
  rxFlag = true;
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
    message += "\nUptime:" + String(millis()/1000) + "s";
  }
  
  both.printf("TX %s ", message.c_str());
  
  #ifndef HELTEC_NO_RADIO_INSTANCE
    // Disable interrupt during transmission
    radio.clearDio1Action();
    
    // LED on during transmission
    heltec_led(50);
    
    // Measure transmission time for duty cycle calculation
    txDuration = millis();
    int state = radio.transmit(message);
    txDuration = millis() - txDuration;
    
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
    
    // Resume listening
    radio.setDio1Action(rx);
    RADIOLIB_OR_HALT(radio.startReceive(RADIOLIB_SX126X_RX_TIMEOUT_INF));
  #else
    both.println("\nRadio not available");
  #endif
}

void handleReceivedPacket() {
  rxFlag = false; // Clear the flag
  
  #ifndef HELTEC_NO_RADIO_INSTANCE
    // Read the received data
    int state = radio.readData(rxdata);
    
    if (state == RADIOLIB_ERR_NONE) {
      heltec_clear_display(2, 1);
      both.printf("RX %s", rxdata.c_str());
      both.printf("\nRSSI: %.1f dBm", radio.getRSSI());
      both.printf("\nSNR: %.1f dB", radio.getSNR());
      both.println("\n---");
      
      // Brief LED flash for received packet
      heltec_led(25);
      delay(100);
      heltec_led(0);
    } else {
      heltec_clear_display(2, 1);
      both.printf("RX Error: %i\n", state);
    }
    
    // Continue listening
    RADIOLIB_OR_HALT(radio.startReceive(RADIOLIB_SX126X_RX_TIMEOUT_INF));
  #endif
}

void setup() {
  // Initialize serial for debug output
  Serial.begin(115200);
  
  // Initialize Heltec library
  heltec_setup();
  
  // Display board information
  heltec_clear_display(1, 1);
  both.println("Heltec LoRa Transceiver");
  
  heltec_clear_display(1, 1);
  both.println("Initializing...");
  
  #ifndef HELTEC_NO_RADIO_INSTANCE
    // Initialize radio
    RADIOLIB_OR_HALT(radio.begin());
    
    // Set interrupt callback for received packets
    radio.setDio1Action(rx);
    
    // Configure radio parameters
    both.printf("Frequency: %.1f MHz\n", FREQUENCY);
    RADIOLIB_OR_HALT(radio.setFrequency(FREQUENCY));
    
    both.printf("Bandwidth: %.1f kHz\n", BANDWIDTH);
    RADIOLIB_OR_HALT(radio.setBandwidth(BANDWIDTH));
    
    both.printf("Spreading Factor: %i\n", SPREADING_FACTOR);
    RADIOLIB_OR_HALT(radio.setSpreadingFactor(SPREADING_FACTOR));
    
    both.printf("TX Power: %i dBm\n", TRANSMIT_POWER);
    RADIOLIB_OR_HALT(radio.setOutputPower(TRANSMIT_POWER));
    
    // Start listening for packets
    both.println("Starting to listen...");
    RADIOLIB_OR_HALT(radio.startReceive(RADIOLIB_SX126X_RX_TIMEOUT_INF));
  #else
    both.println("No radio available");
  #endif
  
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
      heltec_clear_display(2, 1);
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
      heltec_clear_display(2,1);
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
    heltec_clear_display(2, 1);
    transmitPacket(false);
  }
  
  // Handle received packet
  if (rxFlag) {
    handleReceivedPacket();
  }
}