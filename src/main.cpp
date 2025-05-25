/**
 * Heltec Wireless Tracker - Bidirectional LoRa Transceiver
 * - Sends packets automatically every PAUSE seconds
 * - Can send immediately with button press
 * - Always listening for incoming packets
 * - Shows RSSI/SNR for received packets
 * - Includes 1% duty cycle compliance
 * - Built for Heltec Wireless Tracker board
 */

#define HELTEC_POWER_BUTTON     // Enable power button functionality
#define HELTEC_WIRELESS_TRACKER // Specify the board type
#include <heltec_unofficial.h>

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

// Interrupt callback - keep this minimal and fast!
void rx() {
  rxFlag = true;
}
void initializeDisplay(uint8_t textSize = 1, uint8_t rotation = 1) {
    display.fillScreen(ST7735_BLACK);       // Clear the screen to black
    display.setTextColor(ST7735_WHITE);     // Set text color to white
    display.setCursor(0, 0);                // Set cursor to the top-left corner
    display.setRotation(rotation);          // Rotate the display
    display.setTextSize(textSize);          // Set text size
}

void initializeGPS() {
  initializeDisplay();
  if (!gpsInitialized) {
    both.println("Initializing GPS...");
    heltec_gnss_begin();
    gpsInitialized = true;
    both.println("GPS initialized");
    delay(1000); // Give GPS a moment to start
  }
}
void transmitPacket(bool sendGPS = false) {
    String message;
  
  if (sendGPS) {
    // Create GPS location message
    message = "GPS Loc";
    message += "\nLat:" + String(heltec_gnss_latitude(), 6);
    message += "\nLon:" + String(heltec_gnss_longitude(), 6);
    message += "\nAlt:" + String(heltec_gnss_altitude_meters(), 1) + "m";
    message += "\nSats:" + String(heltec_gnss_satellites());
  } else {
    // Create regular status message
    message = "Msg# " + String(messageCounter++);
    message += "\nBat:" + String(heltec_battery_percent()) + "%";
    message += "\nTemp:" + String(heltec_temperature(), 1) + "C";
    message += "\nUptime:" + String(millis()/1000) + "s";
  }
  
  both.printf("TX %s ", message.c_str());
  
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
}

void handleReceivedPacket() {
  rxFlag = false; // Clear the flag
  
  // Read the received data
  int state = radio.readData(rxdata);
  
  if (state == RADIOLIB_ERR_NONE) {
    initializeDisplay(2);
    both.printf("RX [%s]\n", rxdata.c_str());
    both.printf("\nRSSI: %.1f dBm\n", radio.getRSSI());
    both.printf("\nSNR: %.1f dB\n", radio.getSNR());
    both.println("\n---");
    
    // Brief LED flash for received packet
    heltec_led(25);
    delay(100);
    heltec_led(0);
  } else {
    initializeDisplay(2);
    both.printf("RX Error: %i\n", state);
  }
  
  // Continue listening
  RADIOLIB_OR_HALT(radio.startReceive(RADIOLIB_SX126X_RX_TIMEOUT_INF));
}

void setup() {
  heltec_setup();
  initializeDisplay(1);

  both.println("Heltec Tracker. Init...");
  
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
  
  if (PAUSE > 0) {
    both.printf("Auto-tx every %i sec\n", PAUSE);
  } else {
    both.println("Manual tx (press button)");
  }
  both.println("Button = GPS location");
  both.println("Wireless Tracker ready!");
  both.println("---");
}

void loop() {
  heltec_loop();
  
  bool txLegal = millis() > lastTxTime + minimumPause;
  bool timeToTransmit = (PAUSE > 0) && (millis() - lastTxTime > (PAUSE * 1000));
  bool buttonPressed = button.isSingleClick();   // register button press
 
  if (buttonPressed) { // if button is pressed, we'll send GPS info
    initializeGPS();
    
    // Check GPS status after initialization
    if (gpsInitialized) {
      heltec_gnss_update();
      int sats = heltec_gnss_satellites();
      
      if (sats == 0) {
        // No satellites - just show message and don't queue transmission
        initializeDisplay(2);
        both.println("GPS Status");
        both.println("No satellites");
        both.println("acquired");
        delay(2000);
        return; // Exit early, don't set pendingGpsTransmit
      } else {
        // Has satellites - queue the transmission
        pendingGpsTransmit = true;
        initializeDisplay(2);
        both.printf("GPS ready\n%i satellites\nQueued for TX", sats);
        delay(1000);
      }
    }
  }
  
  if (pendingGpsTransmit && txLegal) {
    if (gpsInitialized) {
      heltec_gnss_update();
    }
    initializeDisplay(2);
    transmitPacket(true); 
    pendingGpsTransmit = false;
  } else if (timeToTransmit && txLegal) {
    initializeDisplay(2);
    transmitPacket(false);
  } else if (pendingGpsTransmit && !txLegal) {
    int waitSeconds = ((minimumPause - (millis() - lastTxTime)) / 1000) + 1;
    initializeDisplay(2);
    both.printf("GPS Tx queued\nDuty cycle\nWait %i secs", waitSeconds);
  }
  
  if (rxFlag) {
    handleReceivedPacket();
  }
}
