#ifndef _WEB_H_
#define _WEB_H_

#include <stdint.h>
#include <ESPAsyncWebServer.h>

namespace WEB {

// Initialize web server (call after WiFi is connected)
void begin();

// Get reference to the web server instance
AsyncWebServer& getServer();

// Update live data for WebSocket clients
void updateLiveData();

} // namespace WEB

#endif
