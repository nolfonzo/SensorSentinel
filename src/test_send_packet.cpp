/**
 * Simplified Heltec LoRa Packet Sender
 * - Based on the bidirectional transceiver pattern
 * - Uses proper duty cycle calculation
 */

// Enable power button functionality
#define HELTEC_POWER_BUTTON

// Include the Heltec library
#include "heltec_unofficial.h"
#include "driver/gpio.h"

// Configuration
#define DUTY_CYCLE_PERCENT  1.0       // 1% duty cycle (legal requirement in many regions)
#define MIN_SEND_INTERVAL   5000      // Minimum seconds between transmissions regardless of duty cycle

// Global variables
long messageCounter = 0;
uint64_t lastTxTime = 0;
uint64_t txDuration;
uint64_t minimumPause;                 // For duty cycle compliance

void transmitPacket() {
  // Create regular status message
  String message = "#" + String(messageCounter++);
  message += "\nBat:" + String(heltec_battery_percent()) + "%";
  message += "\nTemp:" + String(heltec_temperature(), 1) + "C";
  message += "\nUp:" + String(millis()/1000) + "s";
  
  heltec_clear_display(2, 1);
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
    
    // Calculate minimum pause for duty cycle compliance
    // Formula: airtime * (100/duty_cycle - 1)
    minimumPause = txDuration * (100.0/DUTY_CYCLE_PERCENT - 1);
    
    // Ensure we respect the minimum interval as well
    if (minimumPause < MIN_SEND_INTERVAL) {
      minimumPause = MIN_SEND_INTERVAL;
    }
    
    // Display when the next transmission can occur
    int nextTxSec = (minimumPause / 1000);
    both.printf("Next TX in %d sec\n", nextTxSec);
    
    lastTxTime = millis();
  #else
    both.println("\nRadio not available");
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
  both.println("Heltec LoRa Sender");
  
  heltec_clear_display(1, 1);
  both.println("Initializing...");
  
  #ifndef HELTEC_NO_RADIO_INSTANCE
    // Initialize radio
    int state = radio.begin();
    if (state == RADIOLIB_ERR_NONE) {
      both.println("Radio initialized!");
    } else {
      both.printf("Radio init failed: %d\n", state);
      while (true) { delay(1000); }  // Hang if radio fails
    }
  #else
    both.println("No radio available");
  #endif
  
  both.printf("Auto-tx using %0.1f%% duty cycle\n", DUTY_CYCLE_PERCENT);
  both.println("Button = Manual TX");
  both.println("Ready!");
}

void loop() {
  // Handle button, power control, and display updates
  heltec_loop();
  
  bool txLegal = millis() > lastTxTime + minimumPause;
  bool timeToTransmit = txLegal && lastTxTime > 0; // Auto-transmit when legal
  bool buttonPressed = heltec_button_clicked();    // Using the library function

  // Handle button press - trigger manual transmission
  if (buttonPressed && txLegal) {
    transmitPacket();
  } else if (buttonPressed && !txLegal) {
    int waitSeconds = ((minimumPause - (millis() - lastTxTime)) / 1000) + 1;
    heltec_clear_display(2,1);
    both.printf("Tx queued\nDuty cycle\nWait %i secs", waitSeconds);
  }
  
  // Handle automatic transmission based on duty cycle
  if (timeToTransmit) {
    transmitPacket();
  }
  
  // If this is first run, start the transmission cycle
  if (lastTxTime == 0) {
    transmitPacket();
  }
}