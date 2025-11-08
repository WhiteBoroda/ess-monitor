// This file will be included into web.cpp as raw HTML
// Complete web interface with all settings

const char HTML_PAGE[] PROGMEM = R"rawliteral(
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
    h2 { color: #4CAF50; margin-bottom: 15px; font-size: 1.5em; }

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
      padding: 20px;
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

    .form-group {
      margin-bottom: 20px;
    }
    .form-group label {
      display: block;
      margin-bottom: 5px;
      color: #888;
    }
    .form-group input[type="text"],
    .form-group input[type="password"],
    .form-group input[type="number"] {
      width: 100%;
      padding: 10px;
      background: #0f0f0f;
      border: 1px solid #333;
      border-radius: 4px;
      color: #e0e0e0;
      font-size: 1em;
    }
    .form-group input[type="checkbox"] {
      margin-right: 10px;
    }
    .form-group small {
      display: block;
      color: #666;
      margin-top: 5px;
      font-size: 0.85em;
    }

    .btn-primary, .btn-danger {
      padding: 12px 24px;
      border: none;
      border-radius: 4px;
      font-size: 1em;
      cursor: pointer;
      transition: all 0.3s;
    }
    .btn-primary {
      background: #4CAF50;
      color: #000;
    }
    .btn-primary:hover {
      background: #45a049;
    }
    .btn-danger {
      background: #F44336;
      color: #fff;
    }
    .btn-danger:hover {
      background: #da190b;
    }

    table {
      width: 100%;
      border-collapse: collapse;
    }
    table td {
      padding: 10px 0;
      border-bottom: 1px solid #333;
    }
    table td:first-child {
      color: #888;
      width: 40%;
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
      <button onclick="showTab('wifi')">WiFi</button>
      <button onclick="showTab('telegram')">Telegram</button>
      <button onclick="showTab('mqtt')">MQTT</button>
      <button onclick="showTab('watchdog')">Watchdog</button>
      <button onclick="showTab('system')">System</button>
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

    <!-- WiFi Settings Tab -->
    <div id="wifi" class="tab-content">
      <div class="card">
        <h2>WiFi Settings</h2>
        <p style="margin-bottom:20px; color: #888;">Configure WiFi connection. Device will reboot after saving.</p>
        <form id="wifiForm" onsubmit="saveSettings(event, 'wifi')">
          <div class="form-group">
            <label>
              <input type="checkbox" id="wifiSTA"> Connect to existing network
            </label>
          </div>
          <div class="form-group">
            <label>SSID:</label>
            <input type="text" id="wifiSSID" maxlength="128">
          </div>
          <div class="form-group">
            <label>Password:</label>
            <input type="password" id="wifiPass" maxlength="128">
          </div>
          <button type="submit" class="btn-primary">Save & Reboot</button>
        </form>
      </div>
    </div>

    <!-- Telegram Settings Tab -->
    <div id="telegram" class="tab-content">
      <div class="card">
        <h2>Telegram Notifications</h2>
        <p style="margin-bottom:20px; color: #888;">Configure Telegram bot for battery alerts.</p>
        <form id="telegramForm" onsubmit="saveSettings(event, 'telegram')">
          <div class="form-group">
            <label>
              <input type="checkbox" id="tgEnabled"> Enable Telegram notifications
            </label>
          </div>
          <div class="form-group">
            <label>Bot Token:</label>
            <input type="text" id="tgBotToken" maxlength="64">
            <small>Get token from @BotFather</small>
          </div>
          <div class="form-group">
            <label>Chat ID:</label>
            <input type="text" id="tgChatID" maxlength="32">
            <small>Your Telegram chat ID</small>
          </div>
          <div class="form-group">
            <label>Current threshold (Amps):</label>
            <input type="number" id="tgThreshold" min="0" max="100" value="2">
            <small>Alert if current exceeds this value</small>
          </div>
          <button type="submit" class="btn-primary">Save & Reboot</button>
        </form>
      </div>
    </div>

    <!-- MQTT Settings Tab -->
    <div id="mqtt" class="tab-content">
      <div class="card">
        <h2>MQTT / Home Assistant</h2>
        <p style="margin-bottom:20px; color: #888;">Configure MQTT broker for Home Assistant integration.</p>
        <form id="mqttForm" onsubmit="saveSettings(event, 'mqtt')">
          <div class="form-group">
            <label>
              <input type="checkbox" id="mqttEnabled"> Enable MQTT
            </label>
          </div>
          <div class="form-group">
            <label>Broker IP:</label>
            <input type="text" id="mqttBroker" maxlength="16">
          </div>
          <div class="form-group">
            <label>Port:</label>
            <input type="number" id="mqttPort" min="1" max="65535" value="1883">
          </div>
          <div class="form-group">
            <label>Username (optional):</label>
            <input type="text" id="mqttUser" maxlength="64">
          </div>
          <div class="form-group">
            <label>Password (optional):</label>
            <input type="password" id="mqttPass" maxlength="64">
          </div>
          <button type="submit" class="btn-primary">Save & Reboot</button>
        </form>
      </div>
    </div>

    <!-- Watchdog Settings Tab -->
    <div id="watchdog" class="tab-content">
      <div class="card">
        <h2>Hardware Watchdog Timer</h2>
        <p style="margin-bottom:20px; color: #888;">Watchdog automatically reboots ESP32 if it freezes.</p>
        <form id="watchdogForm" onsubmit="saveSettings(event, 'watchdog')">
          <div class="form-group">
            <label>
              <input type="checkbox" id="wdEnabled" checked> Enable Hardware Watchdog
            </label>
          </div>
          <div class="form-group">
            <label>Timeout (seconds):</label>
            <input type="number" id="wdTimeout" min="10" max="120" value="60">
            <small>Recommended: 60 seconds</small>
          </div>
          <button type="submit" class="btn-primary">Save & Reboot</button>
        </form>
      </div>
    </div>

    <!-- System Tab -->
    <div id="system" class="tab-content">
      <div class="card">
        <h2>System Information</h2>
        <table style="margin-top:20px;">
          <tr><td>Hostname:</td><td id="sysHostname">--</td></tr>
          <tr><td>IP Address:</td><td id="sysIP">--</td></tr>
          <tr><td>WiFi SSID:</td><td id="sysSSID">--</td></tr>
          <tr><td>WiFi Signal:</td><td id="sysRSSI">--</td></tr>
          <tr><td>Version:</td><td id="sysVersion">--</td></tr>
          <tr><td>Uptime:</td><td id="sysUptime">--</td></tr>
          <tr><td>Free Heap:</td><td id="sysFreeHeap">--</td></tr>
        </table>
        <br>
        <button onclick="rebootDevice()" class="btn-danger">Reboot Device</button>
      </div>
    </div>
  </div>

  <script>
    let ws;

    function connectWs() {
      ws = new WebSocket('ws://' + window.location.host + '/ws');
      ws.onopen = () => {
        document.getElementById('connStatus').textContent = 'Connected';
        document.getElementById('connStatus').className = 'connection-status connected';
        loadAllSettings();
      };
      ws.onclose = () => {
        document.getElementById('connStatus').textContent = 'Disconnected';
        document.getElementById('connStatus').className = 'connection-status disconnected';
        setTimeout(connectWs, 3000);
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
      if (data.health !== undefined) document.getElementById('health').textContent = data.health + '%';
      if (data.voltage !== undefined) document.getElementById('voltage').textContent = data.voltage.toFixed(2) + ' V';
      if (data.current !== undefined) document.getElementById('current').textContent = data.current.toFixed(2) + ' A';
      if (data.temperature !== undefined) document.getElementById('temperature').textContent = data.temperature.toFixed(1) + ' °C';
      if (data.canStatus !== undefined) document.getElementById('canStatus').textContent = data.canStatus;

      // System info
      if (data.hostname !== undefined) {
        document.getElementById('sysHostname').textContent = data.hostname;
      }
      if (data.ip !== undefined) {
        document.getElementById('sysIP').textContent = data.ip;
      }
      if (data.ssid !== undefined) {
        document.getElementById('sysSSID').textContent = data.ssid;
      }
      if (data.rssi !== undefined) {
        document.getElementById('sysRSSI').textContent = data.rssi + ' dBm';
      }
      if (data.version !== undefined) {
        document.getElementById('sysVersion').textContent = data.version;
      }
      if (data.uptime !== undefined) {
        document.getElementById('sysUptime').textContent = data.uptime;
      }
      if (data.freeHeap !== undefined) {
        document.getElementById('sysFreeHeap').textContent = (data.freeHeap / 1024).toFixed(1) + ' KB';
      }
    }

    function showTab(tabName) {
      document.querySelectorAll('.tab-content').forEach(el => el.classList.remove('active'));
      document.querySelectorAll('.nav button').forEach(el => el.classList.remove('active'));
      document.getElementById(tabName).classList.add('active');
      event.target.classList.add('active');
    }

    function loadAllSettings() {
      fetch('/api/settings')
        .then(r => r.json())
        .then(data => {
          // WiFi
          if (data.wifiSTA !== undefined) document.getElementById('wifiSTA').checked = data.wifiSTA;
          if (data.wifiSSID !== undefined) document.getElementById('wifiSSID').value = data.wifiSSID;

          // Telegram
          if (data.tgEnabled !== undefined) document.getElementById('tgEnabled').checked = data.tgEnabled;
          if (data.tgBotToken !== undefined) document.getElementById('tgBotToken').value = data.tgBotToken;
          if (data.tgChatID !== undefined) document.getElementById('tgChatID').value = data.tgChatID;
          if (data.tgThreshold !== undefined) document.getElementById('tgThreshold').value = data.tgThreshold;

          // MQTT
          if (data.mqttEnabled !== undefined) document.getElementById('mqttEnabled').checked = data.mqttEnabled;
          if (data.mqttBroker !== undefined) document.getElementById('mqttBroker').value = data.mqttBroker;
          if (data.mqttPort !== undefined) document.getElementById('mqttPort').value = data.mqttPort;
          if (data.mqttUser !== undefined) document.getElementById('mqttUser').value = data.mqttUser;

          // Watchdog
          if (data.wdEnabled !== undefined) document.getElementById('wdEnabled').checked = data.wdEnabled;
          if (data.wdTimeout !== undefined) document.getElementById('wdTimeout').value = data.wdTimeout;
        })
        .catch(err => console.error('Failed to load settings:', err));
    }

    function saveSettings(event, section) {
      event.preventDefault();

      let data = {};
      if (section === 'wifi') {
        data = {
          wifiSTA: document.getElementById('wifiSTA').checked,
          wifiSSID: document.getElementById('wifiSSID').value,
          wifiPass: document.getElementById('wifiPass').value
        };
      } else if (section === 'telegram') {
        data = {
          tgEnabled: document.getElementById('tgEnabled').checked,
          tgBotToken: document.getElementById('tgBotToken').value,
          tgChatID: document.getElementById('tgChatID').value,
          tgThreshold: parseInt(document.getElementById('tgThreshold').value)
        };
      } else if (section === 'mqtt') {
        data = {
          mqttEnabled: document.getElementById('mqttEnabled').checked,
          mqttBroker: document.getElementById('mqttBroker').value,
          mqttPort: parseInt(document.getElementById('mqttPort').value),
          mqttUser: document.getElementById('mqttUser').value,
          mqttPass: document.getElementById('mqttPass').value
        };
      } else if (section === 'watchdog') {
        data = {
          wdEnabled: document.getElementById('wdEnabled').checked,
          wdTimeout: parseInt(document.getElementById('wdTimeout').value)
        };
      }

      fetch('/api/settings/' + section, {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(data)
      })
      .then(r => r.json())
      .then(result => {
        if (result.success) {
          alert('Settings saved! Device will reboot...');
        } else {
          alert('Failed to save settings: ' + result.error);
        }
      })
      .catch(err => alert('Error: ' + err));
    }

    function rebootDevice() {
      if (confirm('Are you sure you want to reboot the device?')) {
        fetch('/api/reboot', {method: 'POST'})
          .then(() => alert('Device is rebooting...'))
          .catch(err => alert('Reboot failed: ' + err));
      }
    }

    connectWs();
  </script>
</body>
</html>
)rawliteral";
