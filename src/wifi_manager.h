#ifndef _WIFI_MANAGER_H_
#define _WIFI_MANAGER_H_

#include <Arduino.h>

namespace WiFiMgr {

// Initialize WiFi with AsyncWiFiManager
// Returns true if connected to WiFi, false if in AP mode
bool begin();

// Check if WiFi is connected
bool isConnected();

// Get IP address as string
String getIP();

// Get AP SSID (if in AP mode)
String getAPSSID();

// Reset WiFi settings and restart
void reset();

} // namespace WiFiMgr

#endif
