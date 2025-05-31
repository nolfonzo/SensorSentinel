/**
 * Heltec LoRa Transmitter
 * - Sends standardized status packets
 * - Compatible with MQTT gateways
 * - Duty-cycle compliant
 */

#define HELTEC_POWER_BUTTON  // Enable power button functionality
#include "heltec_unofficial.h"

// Configuration
#define PAUSE            10   // Seconds between transmissions (0 = as fast as duty cycle allows)
#define DUTY_CYCLE_PCT   1    // Duty cycle percentage (1% = airtime * 100)

// Global variables
uint32_t messageCounter = 0;
uint64_t lastTxTime = 0;
uint64_t minimumPause = 0;

void transmitPacket() {
  // Prepare to transmit
  heltec_clear_display(1, 1);
  both.printf("Tx #%d", messageCounter);
  
  // Create and build the packet
  heltec_status_packet_t packet;
  size_t packetSize = heltec_build_status_packet(&packet, messageCounter, false);
  
  // Display packet details before transmitting
  both.println("\nContents:");
  heltec_print_packet_info(&packet, false);
  
  // Clear interrupt during transmission
  radio.clearDio1Action();
  
  // Turn on LED during transmission
  heltec_led(50);
  
  // Transmit and measure duration
  uint64_t txStart = millis();

  // DEBUG: Print exactly what we're passing to transmit
  Serial.printf("Transmitting %d bytes to radio.transmit()\n", packetSize);


  int status = radio.transmit((uint8_t*)&packet, packetSize);
  uint64_t txDuration = millis() - txStart;
  
  // Turn off LED
  heltec_led(0);
  
  if (status == RADIOLIB_ERR_NONE) {
    // Show transmission results
    both.printf("TX OK (%u ms)\n", (unsigned int)txDuration);
    
    // Calculate minimum pause based on duty cycle
    minimumPause = txDuration * (100 / DUTY_CYCLE_PCT);
    both.printf("Next TX in %.1f sec\n", minimumPause/1000.0);
    
    // Flash LED to indicate success
    heltec_led(25);
    delay(100);
    heltec_led(0);
    
    // Update counters
    lastTxTime = millis();
    messageCounter++;
  } else {
    both.printf("TX FAIL (%d)\n", status);
    delay(2000);  // Wait before clearing error
  }
}

void setup() {
  // Initialize Heltec library - handles all board-specific setup
  heltec_setup();
  
  // Display welcome info
  heltec_clear_display(1, 1);
  both.println("LoRa Transmitter");
  both.printf("Board: %s\n", heltec_get_board_name());
  both.printf("Interval: %d sec\n", PAUSE);
  both.printf("Duty cycle: %d%%\n", DUTY_CYCLE_PCT);
  
  // Display battery and system info
  both.printf("Battery: %.1f%%\n", heltec_battery_percent());
  both.printf("Temp: %.1f°C\n", heltec_temperature());
  
  delay(2000);
  both.println("Ready to transmit!");
}

void loop() {
  // Handle system tasks
  heltec_loop();
  
  // Check if it's time to transmit
  bool dutyCycleAllows = (millis() > lastTxTime + minimumPause);
  bool intervalElapsed = (PAUSE == 0) || (millis() - lastTxTime > (PAUSE * 1000));
  bool buttonPressed = heltec_button_clicked();
  
  // Handle button press
  if (buttonPressed && dutyCycleAllows) {
    transmitPacket();
  } else if (buttonPressed) {
    // Button pressed but duty cycle doesn't allow transmission yet
    int waitSec = ((lastTxTime + minimumPause) - millis()) / 1000;
    
    heltec_clear_display(1, 1);
    both.printf("Duty cycle limit\n");
    both.printf("Please wait %d sec\n", waitSec);
    delay(2000);
  }
  
  // Handle automatic transmission
  if (intervalElapsed && dutyCycleAllows && !buttonPressed) {
    transmitPacket();
  }
  
  // Show status periodically
  static uint32_t lastStatusUpdate = 0;
  if (millis() - lastStatusUpdate > 5000) {
    lastStatusUpdate = millis();
    
    // Only update if not immediately after a transmission
    if (millis() - lastTxTime > 2000) {
      heltec_clear_display(1, 1);  // Keep header
      
      // Show transmission stats
      both.printf("Last TX: #%d\n", messageCounter - 1);
      both.printf("Uptime: %u sec\n", millis()/1000);
      both.printf("Battery: %.1f%%\n", heltec_battery_percent());
      
      // Show countdown if waiting
      if (lastTxTime > 0) {
        if (millis() < lastTxTime + minimumPause) {
          // Duty cycle waiting
          int waitSec = ((lastTxTime + minimumPause) - millis()) / 1000;
          both.printf("Duty cycle: %d sec\n", waitSec);
        } else if (PAUSE > 0) {
          // Interval waiting
          int waitSec = ((lastTxTime + (PAUSE * 1000)) - millis()) / 1000;
          waitSec = waitSec < 0 ? 0 : waitSec;
          both.printf("Next TX in: %d sec\n", waitSec);
        } else {
          both.println("Ready to transmit");
        }
      }
    }
  }
}