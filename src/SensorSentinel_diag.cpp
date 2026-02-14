/**
 * @file SensorSentinel_diag.cpp
 * @brief On-demand WiFi diagnostic mode implementation
 */

#include "SensorSentinel_diag.h"
#include "heltec_unofficial_revised.h"
#include "SensorSentinel_pins_helper.h"
#include "SensorSentinel_packet_helper.h"
#include <WiFi.h>
#include <WebServer.h>

static WebServer server(80);

static String buildPage() {
  uint32_t nodeId = SensorSentinel_generate_node_id();
  float vbat = heltec_vbat();
  int pct = heltec_battery_percent(vbat);
  unsigned long uptimeSec = millis() / 1000;

  String html = "<!DOCTYPE html><html><head>"
    "<meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<meta http-equiv='refresh' content='5'>"
    "<title>SensorSentinel Diagnostics</title>"
    "<style>"
    "body{font-family:monospace;background:#1a1a2e;color:#e0e0e0;margin:20px;}"
    "h1{color:#0ff;}h2{color:#0af;margin-top:1.5em;}"
    "table{border-collapse:collapse;margin:8px 0;}"
    "td,th{padding:4px 12px;text-align:left;border-bottom:1px solid #333;}"
    "th{color:#0af;}"
    ".val{color:#0f0;font-weight:bold;}"
    "</style></head><body>";

  html += "<h1>SensorSentinel Diagnostics</h1>";

  // Device info
  html += "<h2>Device</h2><table>";
  html += "<tr><th>Node ID</th><td class='val'>0x" + String(nodeId, HEX) + "</td></tr>";
  html += "<tr><th>Board</th><td class='val'>" + String(heltec_get_board_name()) + "</td></tr>";
  html += "<tr><th>Uptime</th><td class='val'>" + String(uptimeSec) + "s</td></tr>";
  html += "</table>";

  // Battery
  html += "<h2>Battery</h2><table>";
  html += "<tr><th>Voltage</th><td class='val'>" + String(vbat, 2) + " V</td></tr>";
  html += "<tr><th>Level</th><td class='val'>" + String(pct) + "%</td></tr>";
  html += "</table>";

  // Analog pins
  html += "<h2>Analog Pins</h2><table>";
  html += "<tr><th>Pin</th><th>GPIO</th><th>Value</th></tr>";
  for (int i = 0; i < SensorSentinel_ANALOG_COUNT; i++) {
    uint16_t val = analogRead(SensorSentinel_analog_pins[i]);
    html += "<tr><td>A" + String(i) + "</td>";
    html += "<td>" + String(SensorSentinel_analog_pins[i]) + "</td>";
    html += "<td class='val'>" + String(val) + "</td></tr>";
  }
  html += "</table>";

  // Digital pins
  html += "<h2>Digital Pins</h2><table>";
  html += "<tr><th>Pin</th><th>GPIO</th><th>State</th></tr>";
  for (int i = 0; i < SensorSentinel_BOOLEAN_COUNT; i++) {
    pinMode(SensorSentinel_boolean_pins[i], INPUT);
    int state = digitalRead(SensorSentinel_boolean_pins[i]);
    html += "<tr><td>D" + String(i) + "</td>";
    html += "<td>" + String(SensorSentinel_boolean_pins[i]) + "</td>";
    html += "<td class='val'>" + String(state ? "HIGH" : "LOW") + "</td></tr>";
  }
  html += "</table>";

  // LoRa config
#ifndef NO_RADIOLIB
  html += "<h2>LoRa Config</h2><table>";
  html += "<tr><th>Frequency</th><td class='val'>" + String(HELTEC_LORA_FREQ, 1) + " MHz</td></tr>";
  html += "<tr><th>Spreading Factor</th><td class='val'>SF" + String(HELTEC_LORA_SF) + "</td></tr>";
  html += "<tr><th>Bandwidth</th><td class='val'>" + String(HELTEC_LORA_BW, 0) + " kHz</td></tr>";
  html += "</table>";
#endif

  html += "<p style='color:#666;margin-top:2em;'>Auto-refreshes every 5 seconds</p>";
  html += "</body></html>";
  return html;
}

static void handleRoot() {
  server.send(200, "text/html", buildPage());
}

void SensorSentinel_diag_check() {
  // Show prompt on display
  heltec_clear_display();
  both.println("\nHold PRG for");
  both.println("diagnostics...");
  heltec_display_update();

  // Wait 3 seconds, checking button state
  pinMode(BUTTON, INPUT);
  unsigned long start = millis();
  bool held = false;
  while (millis() - start < 3000) {
    if (digitalRead(BUTTON) == LOW) {
      held = true;
      break;
    }
    delay(50);
  }

  if (held) {
    // Wait for button release first
    while (digitalRead(BUTTON) == LOW) {
      delay(50);
    }
    SensorSentinel_diag_run(); // Never returns
  }

  // Not held — continue normal operation
  heltec_clear_display();
  heltec_display_update();
}

void SensorSentinel_diag_run() {
  uint32_t nodeId = SensorSentinel_generate_node_id();

  // Build SSID with last 4 hex digits of nodeId
  char ssid[32];
  snprintf(ssid, sizeof(ssid), "SensorSentinel-%04X", (uint16_t)(nodeId & 0xFFFF));

  heltec_clear_display();
  both.println("\nDIAGNOSTIC MODE");
  both.printf("AP: %s\n", ssid);
  both.println("Pass: sentinel");
  both.println("http://192.168.4.1");
  heltec_display_update();

  // Start WiFi AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, "sentinel");

  // Start web server
  server.on("/", handleRoot);
  server.begin();

  Serial.printf("Diagnostic AP started: %s\n", ssid);
  Serial.println("Web server at http://192.168.4.1");

  // Run forever
  while (true) {
    server.handleClient();
    heltec_loop();
    delay(1);
  }
}
