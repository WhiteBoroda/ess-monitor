#include "wifi_manager.h"
#include "types.h"
#include <ESPmDNS.h>
#include <WiFi.h>
#include <Preferences.h>

extern Config Cfg;
extern Preferences Pref;

namespace WiFiMgr {

bool apMode = false;
String apSSID = "";
const char* apPass = "12345678";

bool begin() {
  Serial.println("\n[WiFi] Initializing WiFi...");

  // 1. Reset WiFi state
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  delay(100);

  // 2. Set mode to AP+STA
  WiFi.mode(WIFI_AP_STA);
  
  // 3. Disable power saving for stability
  WiFi.setSleep(false);

  // Set hostname
  WiFi.setHostname(Cfg.hostname);

  // Setup AP
  apSSID = String(Cfg.hostname);
  Serial.printf("[WiFi] Starting AP: %s (Pass: %s)\n", apSSID.c_str(), apPass);
  WiFi.softAP(apSSID.c_str(), apPass);
  
  Serial.printf("[WiFi] AP IP: %s\n", WiFi.softAPIP().toString().c_str());

  // 4. Connect to STA if enabled
  bool connected = false;
  if (Cfg.wifiSTA) {
    Serial.printf("[WiFi] Connecting to %s...\n", Cfg.wifiSSID);
    WiFi.begin(Cfg.wifiSSID, Cfg.wifiPass);

    // Wait for connection (up to 15 seconds)
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      connected = true;
      Serial.println("[WiFi] Connected!");
      Serial.printf("[WiFi]   SSID: %s\n", WiFi.SSID().c_str());
      Serial.printf("[WiFi]   IP: %s\n", WiFi.localIP().toString().c_str());
      Serial.printf("[WiFi]   Signal: %d dBm\n", WiFi.RSSI());
    } else {
      Serial.println("[WiFi] Failed to connect (timeout).");
    }
  } else {
    Serial.println("[WiFi] STA mode disabled. Running in AP-only mode.");
  }

  // 5. Start mDNS
  if (MDNS.begin(Cfg.hostname)) {
    Serial.printf("[WiFi] mDNS started: http://%s.local\n", Cfg.hostname);
    MDNS.addService("http", "tcp", 80);
  } else {
    Serial.println("[WiFi] mDNS failed to start");
  }

  apMode = !connected;
  return connected;
}

bool isConnected() {
  return WiFi.status() == WL_CONNECTED;
}

String getIP() {
  if (isConnected()) {
    return WiFi.localIP().toString();
  } else {
    return WiFi.softAPIP().toString();
  }
}

String getAPSSID() {
  return apSSID;
}

void reset() {
  Serial.println("[WiFi] Resetting WiFi settings...");
  WiFi.disconnect(true);
  delay(1000);
  ESP.restart();
}

} // namespace WiFiMgr
