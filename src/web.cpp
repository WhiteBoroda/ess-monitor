#include "web.h"
#include "can.h"
#include "types.h"
#include "web_html.h"
#include "ota_html.h"
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WebSerialLite.h>
#include <Preferences.h>
#include <WiFi.h>
#include <Update.h>

extern Config Cfg;
extern Preferences Pref;
extern bool needRestart;
extern RuntimeStatus Runtime;
extern ResetStatus Reset;

namespace WEB {

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// WebSocket event handler
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("[WS] Client #%u connected, Total clients: %d, Free Heap: %d KB\n",
                  client->id(), ws.count(), ESP.getFreeHeap() / 1024);

    // Clean up disconnected clients to save memory
    ws.cleanupClients();

    updateLiveData();
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("[WS] Client #%u disconnected, Total clients: %d\n", client->id(), ws.count());
    ws.cleanupClients();
  }
}

// Initialize web server
void begin() {
  Serial.println("[WEB] Initializing async web server...");

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  WebSerial.begin(&server);
  Serial.println("[WEB] WebSerial initialized at /webserial");

  // Handle commands from WebSerial
  WebSerial.onMessage([](const String& msg) {
    if (msg.isEmpty()) return;

    // Handle commands
    if (msg == "status" || msg == "info") {
      WebSerial.println("\n========== ESS Monitor Status ==========");
      RuntimeStatus runtime = Runtime;
      WebSerial.println("Version: " + String(VERSION));
      WebSerial.println("Hostname: " + String(Cfg.hostname));
      WebSerial.println("Last reset: " + String(Reset.reasonLabel) +
                        " (code " + String(Reset.reasonCode) + ")");
      if (Reset.detail[0] != '\0') {
        WebSerial.println(String("  Hint: ") + Reset.detail);
      }
      WebSerial.println(String("WiFi: ") + (runtime.wifiConnected ? "Connected" : "Disconnected"));
      if (runtime.wifiConnected) {
        WebSerial.println("  SSID: " + WiFi.SSID());
        WebSerial.println("  IP: " + String(runtime.cachedIP));
        WebSerial.println("  Signal: " + String(WiFi.RSSI()) + " dBm");
      }
      WebSerial.println(String("CAN: ") + (CAN::isInitialized() ? "OK" : "ERROR - Module not detected"));
      WebSerial.println("Uptime: " + String(millis() / 1000) + " seconds");
      WebSerial.println("Free Heap: " + String(ESP.getFreeHeap() / 1024) + " KB");
      WebSerial.println("========================================\n");
    } else if (msg == "help") {
      WebSerial.println("\n========================================");
      WebSerial.println("   ESS Monitor - WebSerial Console");
      WebSerial.println("========================================");
      WebSerial.println("Version: " + String(VERSION));
      WebSerial.println("Hostname: " + String(Cfg.hostname));
      WebSerial.println("----------------------------------------");
      WebSerial.println("Available commands:");
      WebSerial.println("  status - Show detailed system status");
      WebSerial.println("  info   - Same as status");
      WebSerial.println("  help   - Show this help message");
      WebSerial.println("----------------------------------------");
      WebSerial.println("All system logs appear here in real-time.");
      WebSerial.println("Type 'status' to see current system state.");
      WebSerial.println("========================================\n");
    } else {
      WebSerial.println("Unknown command: " + msg);
      WebSerial.println("Type 'help' for available commands\n");
    }
  });

  // Serve main page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Clean up disconnected WebSocket clients to free memory
    ws.cleanupClients();

    uint32_t freeHeap = ESP.getFreeHeap();
    Serial.printf("[WEB] Main page requested, Free Heap: %d KB\n", freeHeap / 1024);

    // Check if we have enough memory (need at least 40KB free)
    if (freeHeap < 40000) {
      Serial.println("[WEB] WARNING: Low memory! Sending error page");
      request->send(507, "text/plain", "Insufficient memory. Please wait and retry.");
      return;
    }

    // Create response from PROGMEM
    AsyncWebServerResponse *response = request->beginResponse(String("text/html"), strlen_P(HTML_PAGE),
      [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
        size_t htmlLen = strlen_P(HTML_PAGE);
        if (index >= htmlLen) return 0;

        size_t remaining = htmlLen - index;
        size_t toSend = (remaining > maxLen) ? maxLen : remaining;
        memcpy_P(buffer, HTML_PAGE + index, toSend);
        return toSend;
      });
    request->send(response);
    Serial.printf("[WEB] Main page sent, Free Heap after: %d KB\n", ESP.getFreeHeap() / 1024);
  });

  // OTA Update page
  server.on("/ota_update", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.printf("[WEB] OTA page requested, Free Heap: %d KB\n", ESP.getFreeHeap() / 1024);

    // Create response from PROGMEM
    AsyncWebServerResponse *response = request->beginResponse(String("text/html"), strlen_P(OTA_HTML),
      [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
        size_t htmlLen = strlen_P(OTA_HTML);
        if (index >= htmlLen) return 0;

        size_t remaining = htmlLen - index;
        size_t toSend = (remaining > maxLen) ? maxLen : remaining;
        memcpy_P(buffer, OTA_HTML + index, toSend);
        return toSend;
      });
    request->send(response);
  });

  // OTA Update handler
  server.on("/ota_update", HTTP_POST,
    [](AsyncWebServerRequest *request) {
      // Final response after upload completes
      bool updateSuccessful = !Update.hasError();
      AsyncWebServerResponse *response = request->beginResponse(200, "text/plain",
        updateSuccessful ? "Update Success! Rebooting..." : "Update Failed!");
      response->addHeader("Connection", "close");
      request->send(response);

      if (updateSuccessful) {
        Serial.println("[WEB] OTA Update successful, rebooting...");
        delay(1000);
        ESP.restart();
      } else {
        Serial.printf("[WEB] OTA Update failed, error: %s\n", Update.errorString());
      }
    },
    [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
      // Upload handler - called multiple times with chunks of data
      if (index == 0) {
        Serial.printf("[WEB] OTA Update started: %s\n", filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
          Update.printError(Serial);
        }
      }

      if (len) {
        if (Update.write(data, len) != len) {
          Update.printError(Serial);
        } else {
          // Progress reporting
          static size_t lastPercent = 0;
          size_t percent = (Update.progress() * 100) / Update.size();
          if (percent != lastPercent && percent % 10 == 0) {
            Serial.printf("[WEB] OTA Progress: %d%%\n", percent);
            lastPercent = percent;
          }
        }
      }

      if (final) {
        if (Update.end(true)) {
          Serial.printf("[WEB] OTA Update Success: %u bytes\n", index + len);
        } else {
          Update.printError(Serial);
        }
      }
    }
  );

  // API: Get all settings
  server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
    JsonDocument doc;

    doc["wifiSTA"] = Cfg.wifiSTA;
    doc["wifiSSID"] = Cfg.wifiSSID;
    doc["tgEnabled"] = Cfg.tgEnabled;
    doc["tgBotToken"] = Cfg.tgBotToken;
    doc["tgChatID"] = Cfg.tgChatID;
    doc["tgThreshold"] = Cfg.tgCurrentThreshold;
    doc["mqttEnabled"] = Cfg.mqttEnabled;
    doc["mqttBroker"] = Cfg.mqttBrokerIp;
    doc["mqttPort"] = Cfg.mqttPort;
    doc["mqttUser"] = Cfg.mqttUsername;
    doc["canKeepAlive"] = Cfg.canKeepAliveInterval;
    doc["wdEnabled"] = Cfg.watchdogEnabled;
    doc["wdTimeout"] = Cfg.watchdogTimeout;

    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  // API: Save WiFi settings
  server.on("/api/settings/wifi", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, data, len);

      if (error) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
        return;
      }

      Pref.begin("ess");
      if (doc["wifiSTA"].is<bool>()) {
        Cfg.wifiSTA = doc["wifiSTA"].as<bool>();
        Pref.putBool(CFG_WIFI_STA, Cfg.wifiSTA);
      }
      if (doc["wifiSSID"].is<const char*>()) {
        strlcpy(Cfg.wifiSSID, doc["wifiSSID"].as<const char*>(), sizeof(Cfg.wifiSSID));
        Pref.putString(CFG_WIFI_SSID, Cfg.wifiSSID);
      }
      if (doc["wifiPass"].is<const char*>()) {
        strlcpy(Cfg.wifiPass, doc["wifiPass"].as<const char*>(), sizeof(Cfg.wifiPass));
        Pref.putString(CFG_WIFI_PASS, Cfg.wifiPass);
      }
      Pref.end();

      request->send(200, "application/json", "{\"success\":true}");
      needRestart = true;
    });

  // API: Save Telegram settings
  server.on("/api/settings/telegram", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, data, len);

      if (error) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
        return;
      }

      Pref.begin("ess");
      if (doc["tgEnabled"].is<bool>()) {
        Cfg.tgEnabled = doc["tgEnabled"].as<bool>();
        Pref.putBool(CFG_TG_ENABLED, Cfg.tgEnabled);
      }
      if (doc["tgBotToken"].is<const char*>()) {
        strlcpy(Cfg.tgBotToken, doc["tgBotToken"].as<const char*>(), sizeof(Cfg.tgBotToken));
        Pref.putString(CFG_TG_BOT_TOKEN, Cfg.tgBotToken);
      }
      if (doc["tgChatID"].is<const char*>()) {
        strlcpy(Cfg.tgChatID, doc["tgChatID"].as<const char*>(), sizeof(Cfg.tgChatID));
        Pref.putString(CFG_TG_CHAT_ID, Cfg.tgChatID);
      }
      if (doc["tgThreshold"].is<int>()) {
        Cfg.tgCurrentThreshold = doc["tgThreshold"].as<uint8_t>();
        Pref.putUChar(CFG_TG_CURRENT_THRESHOLD, Cfg.tgCurrentThreshold);
      }
      Pref.end();

      request->send(200, "application/json", "{\"success\":true}");
      needRestart = true;
    });

  // API: Save MQTT settings
  server.on("/api/settings/mqtt", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, data, len);

      if (error) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
        return;
      }

      Pref.begin("ess");
      if (doc["mqttEnabled"].is<bool>()) {
        Cfg.mqttEnabled = doc["mqttEnabled"].as<bool>();
        Pref.putBool(CFG_MQQTT_ENABLED, Cfg.mqttEnabled);
      }
      if (doc["mqttBroker"].is<const char*>()) {
        strlcpy(Cfg.mqttBrokerIp, doc["mqttBroker"].as<const char*>(), sizeof(Cfg.mqttBrokerIp));
        Pref.putString(CFG_MQQTT_BROKER_IP, Cfg.mqttBrokerIp);
      }
      if (doc["mqttPort"].is<int>()) {
        Cfg.mqttPort = doc["mqttPort"].as<uint16_t>();
        Pref.putUShort(CFG_MQQTT_PORT, Cfg.mqttPort);
      }
      if (doc["mqttUser"].is<const char*>()) {
        strlcpy(Cfg.mqttUsername, doc["mqttUser"].as<const char*>(), sizeof(Cfg.mqttUsername));
        Pref.putString(CFG_MQQTT_USERNAME, Cfg.mqttUsername);
      }
      if (doc["mqttPass"].is<const char*>()) {
        strlcpy(Cfg.mqttPassword, doc["mqttPass"].as<const char*>(), sizeof(Cfg.mqttPassword));
        Pref.putString(CFG_MQQTT_PASSWORD, Cfg.mqttPassword);
      }
      Pref.end();

      request->send(200, "application/json", "{\"success\":true}");
      needRestart = true;
    });

  // API: Save Watchdog settings
  server.on("/api/settings/watchdog", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, data, len);

      if (error) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
        return;
      }

      Pref.begin("ess");
      if (doc["wdEnabled"].is<bool>()) {
        Cfg.watchdogEnabled = doc["wdEnabled"].as<bool>();
        Pref.putBool(CFG_WATCHDOG_ENABLED, Cfg.watchdogEnabled);
      }
      if (doc["wdTimeout"].is<int>()) {
        Cfg.watchdogTimeout = doc["wdTimeout"].as<uint8_t>();
        Pref.putUChar(CFG_WATCHDOG_TIMEOUT, Cfg.watchdogTimeout);
      }
      Pref.end();

      request->send(200, "application/json", "{\"success\":true}");
      needRestart = true;
    });

  // API: Save all settings at once
  server.on("/api/settings/all", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, data, len);

      if (error) {
        request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
        return;
      }

      Pref.begin("ess");

      // WiFi settings
      if (doc["wifi"]["wifiSTA"].is<bool>()) {
        Cfg.wifiSTA = doc["wifi"]["wifiSTA"].as<bool>();
        Pref.putBool(CFG_WIFI_STA, Cfg.wifiSTA);
      }
      if (doc["wifi"]["wifiSSID"].is<const char*>()) {
        strlcpy(Cfg.wifiSSID, doc["wifi"]["wifiSSID"].as<const char*>(), sizeof(Cfg.wifiSSID));
        Pref.putString(CFG_WIFI_SSID, Cfg.wifiSSID);
      }
      if (doc["wifi"]["wifiPass"].is<const char*>()) {
        strlcpy(Cfg.wifiPass, doc["wifi"]["wifiPass"].as<const char*>(), sizeof(Cfg.wifiPass));
        Pref.putString(CFG_WIFI_PASS, Cfg.wifiPass);
      }

      // Telegram settings
      if (doc["telegram"]["tgEnabled"].is<bool>()) {
        Cfg.tgEnabled = doc["telegram"]["tgEnabled"].as<bool>();
        Pref.putBool(CFG_TG_ENABLED, Cfg.tgEnabled);
      }
      if (doc["telegram"]["tgBotToken"].is<const char*>()) {
        strlcpy(Cfg.tgBotToken, doc["telegram"]["tgBotToken"].as<const char*>(), sizeof(Cfg.tgBotToken));
        Pref.putString(CFG_TG_BOT_TOKEN, Cfg.tgBotToken);
      }
      if (doc["telegram"]["tgChatID"].is<const char*>()) {
        strlcpy(Cfg.tgChatID, doc["telegram"]["tgChatID"].as<const char*>(), sizeof(Cfg.tgChatID));
        Pref.putString(CFG_TG_CHAT_ID, Cfg.tgChatID);
      }
      if (doc["telegram"]["tgThreshold"].is<int>()) {
        Cfg.tgCurrentThreshold = doc["telegram"]["tgThreshold"].as<uint8_t>();
        Pref.putUChar(CFG_TG_CURRENT_THRESHOLD, Cfg.tgCurrentThreshold);
      }

      // MQTT settings
      if (doc["mqtt"]["mqttEnabled"].is<bool>()) {
        Cfg.mqttEnabled = doc["mqtt"]["mqttEnabled"].as<bool>();
        Pref.putBool(CFG_MQQTT_ENABLED, Cfg.mqttEnabled);
      }
      if (doc["mqtt"]["mqttBroker"].is<const char*>()) {
        strlcpy(Cfg.mqttBrokerIp, doc["mqtt"]["mqttBroker"].as<const char*>(), sizeof(Cfg.mqttBrokerIp));
        Pref.putString(CFG_MQQTT_BROKER_IP, Cfg.mqttBrokerIp);
      }
      if (doc["mqtt"]["mqttPort"].is<int>()) {
        Cfg.mqttPort = doc["mqtt"]["mqttPort"].as<uint16_t>();
        Pref.putUShort(CFG_MQQTT_PORT, Cfg.mqttPort);
      }
      if (doc["mqtt"]["mqttUser"].is<const char*>()) {
        strlcpy(Cfg.mqttUsername, doc["mqtt"]["mqttUser"].as<const char*>(), sizeof(Cfg.mqttUsername));
        Pref.putString(CFG_MQQTT_USERNAME, Cfg.mqttUsername);
      }
      if (doc["mqtt"]["mqttPass"].is<const char*>()) {
        strlcpy(Cfg.mqttPassword, doc["mqtt"]["mqttPass"].as<const char*>(), sizeof(Cfg.mqttPassword));
        Pref.putString(CFG_MQQTT_PASSWORD, Cfg.mqttPassword);
      }

      // CAN settings
      if (doc["can"]["canKeepAlive"].is<int>()) {
        Cfg.canKeepAliveInterval = doc["can"]["canKeepAlive"].as<uint16_t>();
        Pref.putUShort(CFG_CAN_KEEPALIVE_INTERVAL, Cfg.canKeepAliveInterval);
      }

      // Watchdog settings
      if (doc["watchdog"]["wdEnabled"].is<bool>()) {
        Cfg.watchdogEnabled = doc["watchdog"]["wdEnabled"].as<bool>();
        Pref.putBool(CFG_WATCHDOG_ENABLED, Cfg.watchdogEnabled);
      }
      if (doc["watchdog"]["wdTimeout"].is<int>()) {
        Cfg.watchdogTimeout = doc["watchdog"]["wdTimeout"].as<uint8_t>();
        Pref.putUChar(CFG_WATCHDOG_TIMEOUT, Cfg.watchdogTimeout);
      }

      Pref.end();

      request->send(200, "application/json", "{\"success\":true}");
      needRestart = true;
    });

  // API: Reboot
  server.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Rebooting...");
    needRestart = true;
  });

  // API: Live data
  server.on("/api/data", HTTP_GET, [](AsyncWebServerRequest *request) {
    JsonDocument doc;
    EssStatus ess = CAN::getEssStatus();

    doc["charge"] = ess.charge;
    doc["health"] = ess.health;
    doc["voltage"] = ess.voltage;
    doc["current"] = ess.current;
    doc["temperature"] = ess.temperature;
    doc["canStatus"] = CAN::isInitialized() ? "OK" : "ERROR";
    RuntimeStatus runtime = Runtime;
    doc["hostname"] = Cfg.hostname;
    doc["ip"] = runtime.cachedIP;
    if (runtime.wifiConnected) {
      doc["ssid"] = WiFi.SSID();
      doc["rssi"] = WiFi.RSSI();
    } else {
      doc["ssid"] = "Not connected";
      doc["rssi"] = 0;
    }
    doc["wifi"] = runtime.wifiConnected; // mirrored in websocket payload for quick checks on UI
    doc["resetReason"] = Reset.reasonLabel;
    doc["resetCode"] = Reset.reasonCode;
    doc["resetHint"] = Reset.detail;
    doc["version"] = VERSION;
    doc["uptime"] = String(millis() / 1000) + "s";
    doc["freeHeap"] = ESP.getFreeHeap();

    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  server.begin();
  Serial.println("[WEB] ✓ Async web server started on port 80");
  Serial.println("[WEB]   Main page: http://<ip>/");
  Serial.println("[WEB]   Console: http://<ip>/webserial");
}

AsyncWebServer& getServer() {
  return server;
}

void updateLiveData() {
  if (ws.count() == 0) return;

  JsonDocument doc;
  EssStatus ess = CAN::getEssStatus();

  doc["charge"] = ess.charge;
  doc["health"] = ess.health;
  doc["voltage"] = ess.voltage;
  doc["current"] = ess.current;
  doc["temperature"] = ess.temperature;
  doc["canStatus"] = CAN::isInitialized() ? "OK" : "ERROR";
  doc["hostname"] = Cfg.hostname;

  RuntimeStatus runtime = Runtime;
  doc["ip"] = runtime.cachedIP;
  if (runtime.wifiConnected) {
    doc["ssid"] = WiFi.SSID();
    doc["rssi"] = WiFi.RSSI();
  } else {
    doc["ssid"] = "Not connected";
    doc["rssi"] = 0;
  }
  doc["wifi"] = runtime.wifiConnected; // allow front-end to quickly detect WiFi loss

  doc["resetReason"] = Reset.reasonLabel;
  doc["resetCode"] = Reset.reasonCode;
  doc["resetHint"] = Reset.detail;

  doc["version"] = VERSION;
  doc["uptime"] = String(millis() / 1000) + "s";
  doc["freeHeap"] = ESP.getFreeHeap();

  String json;
  serializeJson(doc, json);
  ws.textAll(json);
}

} // namespace WEB
