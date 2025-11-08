#include "web.h"
#include "can.h"
#include "types.h"
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WebSerialLite.h>
#include <Preferences.h>
#include <WiFi.h>

extern Config Cfg;
extern Preferences Pref;
extern bool needRestart;

namespace WEB {

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// WebSocket event handler
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("[WS] Client #%u connected\n", client->id());
    // Send initial data
    updateLiveData();
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("[WS] Client #%u disconnected\n", client->id());
  }
}

// Initialize web server
void begin() {
  Serial.println("[WEB] Initializing async web server...");

  // Add WebSocket handler
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  // Initialize WebSerial for console logs
  WebSerial.begin(&server);
  Serial.println("[WEB] WebSerial initialized at /webserial");

  // Serve main page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESS Monitor</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Arial, sans-serif;
      background: #0f0f0f;
      color: #e0e0e0;
      padding: 20px;
    }
    .container { max-width: 1200px; margin: 0 auto; }
    h1 { color: #4CAF50; margin-bottom: 30px; font-size: 2em; }
    .nav {
      display: flex;
      gap: 10px;
      margin-bottom: 20px;
      flex-wrap: wrap;
    }
    .nav button {
      padding: 10px 20px;
      background: #1e1e1e;
      border: 1px solid #333;
      color: #e0e0e0;
      cursor: pointer;
      border-radius: 4px;
      transition: all 0.3s;
    }
    .nav button:hover { background: #2a2a2a; }
    .nav button.active {
      background: #4CAF50;
      border-color: #4CAF50;
      color: #000;
    }
    .tab-content { display: none; }
    .tab-content.active { display: block; }
    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
      gap: 15px;
      margin-bottom: 20px;
    }
    .card {
      background: #1a1a1a;
      border: 1px solid #333;
      border-radius: 8px;
      padding: 15px;
    }
    .card h3 {
      color: #888;
      font-size: 0.9em;
      margin-bottom: 10px;
      text-transform: uppercase;
    }
    .card .value {
      font-size: 1.8em;
      font-weight: bold;
      color: #4CAF50;
    }
    .status-ok { color: #4CAF50; }
    .status-warning { color: #FF9800; }
    .status-error { color: #F44336; }
    .connection-status {
      position: fixed;
      top: 20px;
      right: 20px;
      padding: 10px 15px;
      border-radius: 4px;
      font-size: 0.9em;
    }
    .connected { background: #4CAF50; color: #000; }
    .disconnected { background: #F44336; color: #fff; }
  </style>
</head>
<body>
  <div class="container">
    <h1>⚡ ESS Monitor</h1>

    <div class="connection-status disconnected" id="connStatus">Disconnected</div>

    <div class="nav">
      <button onclick="showTab('status')" class="active">Status</button>
      <button onclick="showTab('settings')">Settings</button>
      <button onclick="window.open('/webserial', '_blank')">Console</button>
    </div>

    <!-- Status Tab -->
    <div id="status" class="tab-content active">
      <div class="grid">
        <div class="card">
          <h3>Battery Charge</h3>
          <div class="value" id="charge">--%</div>
        </div>
        <div class="card">
          <h3>Health</h3>
          <div class="value" id="health">--%</div>
        </div>
        <div class="card">
          <h3>Voltage</h3>
          <div class="value" id="voltage">-- V</div>
        </div>
        <div class="card">
          <h3>Current</h3>
          <div class="value" id="current">-- A</div>
        </div>
        <div class="card">
          <h3>Temperature</h3>
          <div class="value" id="temperature">-- °C</div>
        </div>
        <div class="card">
          <h3>CAN Status</h3>
          <div class="value" id="canStatus">--</div>
        </div>
      </div>
    </div>

    <!-- Settings Tab -->
    <div id="settings" class="tab-content">
      <div class="card">
        <h3>System Information</h3>
        <p>Hostname: <span id="hostname">--</span></p>
        <p>IP Address: <span id="ipAddr">--</span></p>
        <p>Version: v0.1-dev</p>
        <br>
        <button onclick="rebootDevice()" style="padding: 10px 20px; background: #F44336; border: none; color: white; cursor: pointer; border-radius: 4px;">Reboot Device</button>
      </div>
    </div>
  </div>

  <script>
    let ws;
    let wsReconnectTimer;

    function connectWs() {
      ws = new WebSocket('ws://' + window.location.host + '/ws');

      ws.onopen = () => {
        console.log('WebSocket connected');
        document.getElementById('connStatus').textContent = 'Connected';
        document.getElementById('connStatus').className = 'connection-status connected';
      };

      ws.onclose = () => {
        console.log('WebSocket disconnected');
        document.getElementById('connStatus').textContent = 'Disconnected';
        document.getElementById('connStatus').className = 'connection-status disconnected';
        wsReconnectTimer = setTimeout(connectWs, 3000);
      };

      ws.onmessage = (event) => {
        try {
          const data = JSON.parse(event.data);
          updateData(data);
        } catch (e) {
          console.error('Failed to parse data:', e);
        }
      };
    }

    function updateData(data) {
      if (data.charge !== undefined) {
        document.getElementById('charge').textContent = data.charge + '%';
        document.getElementById('charge').className = data.charge > 20 ? 'value status-ok' : 'value status-warning';
      }
      if (data.health !== undefined) {
        document.getElementById('health').textContent = data.health + '%';
      }
      if (data.voltage !== undefined) {
        document.getElementById('voltage').textContent = data.voltage.toFixed(2) + ' V';
      }
      if (data.current !== undefined) {
        document.getElementById('current').textContent = data.current.toFixed(2) + ' A';
      }
      if (data.temperature !== undefined) {
        document.getElementById('temperature').textContent = data.temperature.toFixed(1) + ' °C';
      }
      if (data.canStatus !== undefined) {
        document.getElementById('canStatus').textContent = data.canStatus;
      }
      if (data.hostname !== undefined) {
        document.getElementById('hostname').textContent = data.hostname;
      }
      if (data.ip !== undefined) {
        document.getElementById('ipAddr').textContent = data.ip;
      }
    }

    function showTab(tabName) {
      document.querySelectorAll('.tab-content').forEach(el => el.classList.remove('active'));
      document.querySelectorAll('.nav button').forEach(el => el.classList.remove('active'));
      document.getElementById(tabName).classList.add('active');
      event.target.classList.add('active');
    }

    function rebootDevice() {
      if (confirm('Are you sure you want to reboot the device?')) {
        fetch('/api/reboot', {method: 'POST'})
          .then(() => alert('Device is rebooting...'))
          .catch(err => alert('Reboot failed: ' + err));
      }
    }

    // Connect on load
    connectWs();
  </script>
</body>
</html>
)rawliteral";
    request->send(200, "text/html", html);
  });

  // API endpoint for reboot
  server.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Rebooting...");
    needRestart = true;
  });

  // API endpoint for live data (JSON)
  server.on("/api/data", HTTP_GET, [](AsyncWebServerRequest *request) {
    StaticJsonDocument<512> doc;
    EssStatus ess = CAN::getEssStatus();

    doc["charge"] = ess.charge;
    doc["health"] = ess.health;
    doc["voltage"] = ess.voltage;
    doc["current"] = ess.current;
    doc["temperature"] = ess.temperature;
    doc["canStatus"] = "OK";
    doc["hostname"] = Cfg.hostname;
    doc["ip"] = WiFi.localIP().toString();

    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  // Start server
  server.begin();
  Serial.println("[WEB] ✓ Async web server started on port 80");
  Serial.println("[WEB]   Main page: http://<ip>/");
  Serial.println("[WEB]   Console: http://<ip>/webserial");
}

AsyncWebServer& getServer() {
  return server;
}

void updateLiveData() {
  if (ws.count() == 0) return; // No clients connected

  StaticJsonDocument<512> doc;
  EssStatus ess = CAN::getEssStatus();

  doc["charge"] = ess.charge;
  doc["health"] = ess.health;
  doc["voltage"] = ess.voltage;
  doc["current"] = ess.current;
  doc["temperature"] = ess.temperature;
  doc["canStatus"] = "OK";
  doc["hostname"] = Cfg.hostname;
  doc["ip"] = WiFi.localIP().toString();

  String json;
  serializeJson(doc, json);
  ws.textAll(json);
}

} // namespace WEB
