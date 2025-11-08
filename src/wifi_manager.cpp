#include "wifi_manager.h"
#include "types.h"
#include <ESPAsyncWiFiManager.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>

extern Config Cfg;

namespace WiFiMgr {

DNSServer dnsServer;
AsyncWebServer configServer(80);  // Temporary server for WiFiManager only
bool apMode = false;
String apSSID = "";

bool begin() {
  Serial.println("\n[WiFi] Initializing WiFi Manager...");

  // Set hostname
  WiFi.setHostname(Cfg.hostname);

  // Create WiFi Manager
  ESPAsyncWiFiManager wifiManager(&configServer, &dnsServer);

  // Set timeout for config portal (3 minutes)
  wifiManager.setConfigPortalTimeout(180);

  // Set minimum signal quality
  wifiManager.setMinimumSignalQuality(10);

  // Custom AP name based on hostname
  apSSID = String(Cfg.hostname) + "-AP";

  Serial.printf("[WiFi] Starting WiFiManager with AP: %s\n", apSSID.c_str());
  Serial.println("[WiFi] If not connected, device will create Access Point");
  Serial.println("[WiFi] Connect to AP and navigate to 192.168.4.1 to configure");

  // Try to connect, if fails - start AP mode
  // autoConnect will:
  // 1. Try to connect to saved WiFi
  // 2. If fails, create AP with captive portal
  // 3. Wait for user to configure WiFi
  // 4. Restart and connect to configured WiFi
  bool connected = wifiManager.autoConnect(apSSID.c_str(), "12345678");

  if (connected) {
    apMode = false;
    Serial.println("[WiFi] ✓ Connected to WiFi!");
    Serial.printf("[WiFi]   SSID: %s\n", WiFi.SSID().c_str());
    Serial.printf("[WiFi]   IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[WiFi]   Signal: %d dBm\n", WiFi.RSSI());
    Serial.printf("[WiFi]   Hostname: %s.local\n", Cfg.hostname);

    // Start mDNS
    if (MDNS.begin(Cfg.hostname)) {
      Serial.printf("[WiFi] ✓ mDNS started: http://%s.local\n", Cfg.hostname);
      MDNS.addService("http", "tcp", 80);
    } else {
      Serial.println("[WiFi] ✗ mDNS failed to start");
    }

    return true;
  } else {
    apMode = true;
    Serial.println("[WiFi] ✗ Failed to connect to WiFi");
    Serial.println("[WiFi] Device is in AP mode - please configure WiFi");
    return false;
  }
}

bool isConnected() {
  return WiFi.status() == WL_CONNECTED;
}

String getIP() {
  if (apMode) {
    return WiFi.softAPIP().toString();
  } else {
    return WiFi.localIP().toString();
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
