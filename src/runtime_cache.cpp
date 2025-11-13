#include "runtime_cache.h"

#include <Arduino.h>
#include <WiFi.h>
#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

namespace RuntimeCache {

namespace {
  portMUX_TYPE runtimeMux = portMUX_INITIALIZER_UNLOCKED;
  RuntimeStatus cachedStatus;
  bool statusInitialized = false;
  bool lastWifiConnected = false;
  uint32_t lastUpdateMillis = 0;

  void setStatus(const RuntimeStatus &status) {
    portENTER_CRITICAL(&runtimeMux);
    cachedStatus = status;
    portEXIT_CRITICAL(&runtimeMux);
    lastWifiConnected = status.wifiConnected;
    statusInitialized = true;
  }
}

void updateFromWiFi() {
  bool connected = (WiFi.status() == WL_CONNECTED);
  uint32_t now = millis();

  // Throttle updates to once per second unless the connection state changes.
  if (statusInitialized && connected == lastWifiConnected &&
      (now - lastUpdateMillis) < 1000) {
    return;
  }

  RuntimeStatus updated = {};
  updated.wifiConnected = connected;

  if (connected) {
    IPAddress ip = WiFi.localIP();
    snprintf(updated.cachedIP, sizeof(updated.cachedIP), "%u.%u.%u.%u",
             ip[0], ip[1], ip[2], ip[3]);

    String ssid = WiFi.SSID();
    strlcpy(updated.cachedSSID, ssid.c_str(), sizeof(updated.cachedSSID));

    updated.wifiRSSI = WiFi.RSSI();
  } else {
    strlcpy(updated.cachedIP, "0.0.0.0", sizeof(updated.cachedIP));
    updated.cachedSSID[0] = '\0';
    updated.wifiRSSI = 0;
  }

  setStatus(updated);
  lastUpdateMillis = now;
}

RuntimeStatus getSnapshot() {
  RuntimeStatus copy;
  portENTER_CRITICAL(&runtimeMux);
  copy = cachedStatus;
  portEXIT_CRITICAL(&runtimeMux);
  return copy;
}

bool isWifiConnected() {
  bool connected;
  portENTER_CRITICAL(&runtimeMux);
  connected = cachedStatus.wifiConnected;
  portEXIT_CRITICAL(&runtimeMux);
  return connected;
}

} // namespace RuntimeCache
