/**
 * @file SensorSentinel_diag.cpp
 * @brief On-demand WiFi diagnostic mode with configurable send interval
 */

#include "SensorSentinel_diag.h"
#include "heltec_unofficial_revised.h"
#include "SensorSentinel_pins_helper.h"
#include "SensorSentinel_packet_helper.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

static WebServer server(80);
static Preferences prefs;

static const char* NVS_NAMESPACE           = "sentinel";
static const char* NVS_KEY_INTERVAL        = "interval";
static const char* NVS_KEY_SENSOR_INTERVAL = "sensor_ivl";
static const char* NVS_KEY_MQTT_SERVER     = "mqtt_srv";
static const int   DEFAULT_INTERVAL        = 30;   // sender sleep interval (s)
static const int   DEFAULT_SENSOR_INTERVAL = 60;   // repeater sensor TX interval (s)

int SensorSentinel_diag_get_interval() {
  prefs.begin(NVS_NAMESPACE, true);
  int val = prefs.getInt(NVS_KEY_INTERVAL, DEFAULT_INTERVAL);
  prefs.end();
  return val;
}

static void SensorSentinel_diag_set_interval(int seconds) {
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putInt(NVS_KEY_INTERVAL, seconds);
  prefs.end();
}

int SensorSentinel_diag_get_sensor_interval() {
  prefs.begin(NVS_NAMESPACE, true);
  int val = prefs.getInt(NVS_KEY_SENSOR_INTERVAL, DEFAULT_SENSOR_INTERVAL);
  prefs.end();
  return val;
}

static void SensorSentinel_diag_set_sensor_interval(int seconds) {
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putInt(NVS_KEY_SENSOR_INTERVAL, seconds);
  prefs.end();
}

String SensorSentinel_diag_get_mqtt_server() {
  prefs.begin(NVS_NAMESPACE, true);
  String val = prefs.getString(NVS_KEY_MQTT_SERVER, MQTT_SERVER);
  prefs.end();
  return val;
}

static void SensorSentinel_diag_set_mqtt_server(const String& server) {
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putString(NVS_KEY_MQTT_SERVER, server);
  prefs.end();
}

static String buildPage() {
  uint32_t nodeId = SensorSentinel_generate_node_id();
  float vbat = heltec_vbat();
  int pct = heltec_battery_percent(vbat);
  unsigned long uptimeSec = millis() / 1000;
  int currentInterval = SensorSentinel_diag_get_interval();

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
    "button{padding:10px 20px;margin:5px;font-size:16px;font-family:monospace;"
    "border:2px solid #0af;background:#1a1a2e;color:#0af;cursor:pointer;}"
    "button:hover{background:#0af;color:#1a1a2e;}"
    "button.active{background:#0f0;color:#1a1a2e;border-color:#0f0;}"
    "</style></head><body>";

  html += "<h1>SensorSentinel Diagnostics</h1>";

#if REPEATER_MODE
  // Repeater: show sensor TX interval
  int currentSensorInterval = SensorSentinel_diag_get_sensor_interval();
  html += "<h2>Sensor Interval</h2>";
  html += "<p>How often this node transmits its own sensor data.</p>";
  html += "<p>Current: <span class='val'>" + String(currentSensorInterval) + "s</span></p>";
  html += "<form method='POST' action='/setmode'>";
  int sensorOpts[] = {30, 60, 300, 600};
  const char* sensorLabels[] = {"30s", "1 min", "5 min", "10 min"};
  for (int i = 0; i < 4; i++) {
    html += "<button type='submit' name='sensor_interval' value='" + String(sensorOpts[i]) + "'";
    if (currentSensorInterval == sensorOpts[i]) html += " class='active'";
    html += ">" + String(sensorLabels[i]) + "</button>";
  }
  html += "</form>";
#else
  // Sender: show sleep interval
  html += "<h2>Send Interval</h2>";
  html += "<p>Current: <span class='val'>" + String(currentInterval) + "s";
  html += (currentInterval <= 30) ? " (Fast)" : " (Slow)";
  html += "</span></p>";
  html += "<form method='POST' action='/setmode'>";
  html += "<button type='submit' name='interval' value='30'";
  if (currentInterval == 30) html += " class='active'";
  html += ">Fast (30s)</button>";
  html += "<button type='submit' name='interval' value='300'";
  if (currentInterval == 300) html += " class='active'";
  html += ">Slow (5min)</button>";
  html += "</form>";
#endif

  // MQTT server
  String currentMqtt = SensorSentinel_diag_get_mqtt_server();
  html += "<h2>MQTT Server</h2>";
  html += "<p>Current: <span class='val'>" + currentMqtt + "</span></p>";
  html += "<form method='POST' action='/setmode'>";
  html += "<input type='text' name='mqtt_server' value='" + currentMqtt + "' "
          "style='font-family:monospace;background:#111;color:#0f0;border:1px solid #0af;"
          "padding:6px;width:260px;font-size:14px;'>";
  html += " <button type='submit'>Set</button>";
  html += "</form>";

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

static void handleSetMode() {
  auto redirectOk = [](String msg) {
    String html = "<!DOCTYPE html><html><head>"
      "<meta charset='utf-8'>"
      "<meta http-equiv='refresh' content='2;url=/'>"
      "<style>body{font-family:monospace;background:#1a1a2e;color:#0f0;"
      "display:flex;justify-content:center;align-items:center;height:100vh;}"
      "</style></head><body><h2>" + msg + " Redirecting...</h2></body></html>";
    server.send(200, "text/html", html);
  };

  if (server.hasArg("interval")) {
    int v = server.arg("interval").toInt();
    if (v == 30 || v == 300) {
      SensorSentinel_diag_set_interval(v);
      Serial.printf("Send interval set to %ds\n", v);
      redirectOk("Interval set to " + String(v) + "s.");
    } else {
      server.send(400, "text/plain", "Invalid interval");
    }
  } else if (server.hasArg("sensor_interval")) {
    int v = server.arg("sensor_interval").toInt();
    if (v == 30 || v == 60 || v == 300 || v == 600) {
      SensorSentinel_diag_set_sensor_interval(v);
      Serial.printf("Sensor interval set to %ds\n", v);
      redirectOk("Sensor interval set to " + String(v) + "s.");
    } else {
      server.send(400, "text/plain", "Invalid sensor interval");
    }
  } else if (server.hasArg("mqtt_server")) {
    String v = server.arg("mqtt_server");
    v.trim();
    if (v.length() > 0 && v.length() < 64) {
      SensorSentinel_diag_set_mqtt_server(v);
      Serial.printf("MQTT server set to %s\n", v.c_str());
      redirectOk("MQTT server set to " + v + ".");
    } else {
      server.send(400, "text/plain", "Invalid MQTT server");
    }
  } else {
    server.send(400, "text/plain", "Missing parameter");
  }
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
  server.on("/setmode", HTTP_POST, handleSetMode);
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
