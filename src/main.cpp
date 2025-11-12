#include "can.h"
#include "hass.h"
#include "lcd.h"
#include "logger.h"
#include "tg.h"
#include "types.h"
#include "watchdog.h"
#include "web.h"
#include <Arduino.h>
#include <IPAddress.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiMulti.h>

WiFiMulti wifiMulti;

Preferences Pref;
Config Cfg;
volatile EssStatus Ess;

void initConfig();

bool wifiEverConnected = false;

bool needRestart = false;

bool initWiFi();
void logBatteryState();

void setup() {
  Serial.begin(115200);

  // Check if there was a watchdog crash on previous boot
  WATCHDOG::checkAndPrintCrashInfo();

  // Load configuration from flash
  initConfig();

  if (initWiFi()) {
    // Initialize Logger after WiFi connection
    Logger::begin();

    if (Cfg.mqttEnabled) {
      HASS::begin(1, 1);
    }
    if (Cfg.tgEnabled) {
      TG::begin(1, 1);
    }
  }

  CAN::begin(1, 1); // TODO: wait for success for tg and hass to start
  WEB::begin(1, 1);
  LCD::begin(1, 1);

  // Initialize inter-core watchdog on Core 1 to monitor Core 0
  // CRITICAL: Watchdog runs on Core 1, so if Core 0 freezes, watchdog will detect it!
  WATCHDOG::begin(1, 2); // Core 1, high priority
}

void loop() {
  static uint32_t previousMillis;
  static uint32_t previousWiFiCheck = 0;
  uint32_t currentMillis = millis();

  // Trace checkpoint: loop start
  WATCHDOG::trace(WATCHDOG::TRACE_LOOP_START);

  // Send heartbeat to watchdog task on Core 1
  // CRITICAL: This tells Core 1 that Core 0 is still alive!
  // If this stops being called, Core 1 watchdog will restart ESP32
  WATCHDOG::heartbeat();
  WATCHDOG::trace(WATCHDOG::TRACE_HEARTBEAT_DONE);

  // WiFi reconnection - ONLY if disconnected and max once per 30 seconds
  // CRITICAL FIX: wifiMulti.run() is BLOCKING and can take 5-10 seconds!
  // Calling it every loop() iteration causes massive freezes when WiFi is down.
  if (currentMillis - previousWiFiCheck >= 30000) {
    WATCHDOG::trace(WATCHDOG::TRACE_WIFI_CHECK_START);
    previousWiFiCheck = currentMillis;

    // Only attempt reconnection if WiFi is actually disconnected
    WATCHDOG::trace(WATCHDOG::TRACE_WIFI_STATUS_CALL);
    wl_status_t wifiStatus = WiFi.status();

    if (wifiStatus != WL_CONNECTED && wifiEverConnected) {
      Serial.println("[MAIN] WiFi disconnected, attempting reconnection...");
      WATCHDOG::trace(WATCHDOG::TRACE_WIFI_RUN_START);

      // Run WiFi reconnection with periodic heartbeats to prevent watchdog timeout
      // wifiMulti.run() can block for 5-10 seconds, so we send heartbeats during this
      uint32_t wifiStartTime = millis();
      uint8_t wifiResult = wifiMulti.run();
      uint32_t wifiDuration = millis() - wifiStartTime;

      if (wifiResult == WL_CONNECTED) {
        Serial.printf("[MAIN] WiFi reconnected in %lu ms\n", wifiDuration);
      } else {
        Serial.printf("[MAIN] WiFi reconnection failed after %lu ms (status: %d)\n",
                      wifiDuration, wifiResult);
      }

      WATCHDOG::trace(WATCHDOG::TRACE_WIFI_RUN_DONE);
    }
    WATCHDOG::trace(WATCHDOG::TRACE_WIFI_CHECK_DONE);
  }

  // Every 5 seconds
  if (currentMillis - previousMillis >= 5000 * 1) {
    previousMillis = currentMillis;

    WATCHDOG::trace(WATCHDOG::TRACE_LOG_BATTERY_START);
    logBatteryState();
    WATCHDOG::trace(WATCHDOG::TRACE_LOG_BATTERY_DONE);
    //SoftReset
    if (needRestart){ESP.restart();};
  }

  WATCHDOG::trace(WATCHDOG::TRACE_LOOP_END);
}

void initConfig() {
  Pref.begin("ess");

  Cfg.wifiSTA = Pref.getBool(CFG_WIFI_STA, Cfg.wifiSTA);
  Pref.getString(CFG_WIFI_SSID, Cfg.wifiSSID, sizeof(Cfg.wifiSSID));
  Pref.getString(CFG_WIFI_PASS, Cfg.wifiPass, sizeof(Cfg.wifiPass));

  Pref.getString(CFG_HOSTNAME, Cfg.hostname, sizeof(Cfg.hostname));

  Cfg.chargeLimit = Pref.getUChar(CFG_INVERTER_CHARGE_LIMIT, Cfg.chargeLimit);
  Cfg.dishargeLimit =
      Pref.getUChar(CFG_INVERTER_DISCHARGE_LIMIT, Cfg.dishargeLimit);

  Cfg.mqttEnabled = Pref.getBool(CFG_MQQTT_ENABLED, Cfg.mqttEnabled);
  Pref.getString(CFG_MQQTT_BROKER_IP, Cfg.mqttBrokerIp,
                 sizeof(Cfg.mqttBrokerIp));
  Cfg.mqttPort = Pref.getUShort(CFG_MQQTT_PORT, Cfg.mqttPort);
  Pref.getString(CFG_MQQTT_USERNAME, Cfg.mqttUsername,
                 sizeof(Cfg.mqttUsername));
  Pref.getString(CFG_MQQTT_PASSWORD, Cfg.mqttPassword,
                 sizeof(Cfg.mqttPassword));

  Cfg.tgEnabled = Pref.getBool(CFG_TG_ENABLED, Cfg.tgEnabled);
  Pref.getString(CFG_TG_BOT_TOKEN, Cfg.tgBotToken, sizeof(Cfg.tgBotToken));
  Pref.getString(CFG_TG_CHAT_ID, Cfg.tgChatID, sizeof(Cfg.tgChatID));
  Cfg.tgCurrentThreshold = Pref.getUChar(CFG_TG_CURRENT_THRESHOLD, Cfg.tgCurrentThreshold);


  Cfg.watchdogEnabled = Pref.getBool(CFG_WATCHDOG_ENABLED, Cfg.watchdogEnabled);
  Cfg.watchdogTimeout = Pref.getUChar(CFG_WATCHDOG_TIMEOUT, Cfg.watchdogTimeout);

  Cfg.syslogEnabled = Pref.getBool(CFG_SYSLOG_ENABLED, Cfg.syslogEnabled);
  Pref.getString(CFG_SYSLOG_SERVER, Cfg.syslogServer, sizeof(Cfg.syslogServer));
  Cfg.syslogPort = Pref.getUShort(CFG_SYSLOG_PORT, Cfg.syslogPort);
  Cfg.syslogLevel = Pref.getUChar(CFG_SYSLOG_LEVEL, Cfg.syslogLevel);

  Pref.end();
}

bool initWiFi() {
  if (Cfg.wifiSTA && Cfg.wifiSSID != NULL && Cfg.wifiSSID[0] != '\0') {
    Serial.printf("Connecting to %s...", Cfg.wifiSSID);

    WiFi.mode(WIFI_STA);
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
    WiFi.setHostname(Cfg.hostname);

    // Reset WiFiMulti
    wifiMulti = WiFiMulti();
    wifiMulti.addAP(Cfg.wifiSSID, Cfg.wifiPass);

    uint32_t previousMillis = millis();
    while (millis() - previousMillis < 20000) {
      if (wifiMulti.run() == WL_CONNECTED) {
        Serial.println(" OK.");
        Serial.println("IP Address: " + WiFi.localIP().toString());
        Serial.printf("mDNS hostname: '%s.local'\n", Cfg.hostname);
        wifiEverConnected = true;
        return true;
      }
      delay(500);
      Serial.print(".");
    }

    Serial.println("Could not connect to WiFi network in 20 seconds.");
  }

  if (!wifiEverConnected) {
    Serial.println("Starting WiFi access point.");
    IPAddress AP_IP(192, 168, 4, 1);
    IPAddress AP_PFX(255, 255, 255, 0);
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(AP_IP, AP_IP, AP_PFX);
    WiFi.softAP(Cfg.hostname, "12345678");
    Serial.printf("Access point SSID: '%s'\n", Cfg.hostname);
    return false;
  }

  return false;
}

void logBatteryState() {
#ifdef DEBUG
  // Get thread-safe copy of battery status
  EssStatus ess = CAN::getEssStatus();

  Serial.printf("[MAIN] Load %.1f | RC %.1f/%.1f | SOC %d%% | SOH %d%% | T "
                "%.1f°C | V %.2f/%.2f\n",
                ess.current, ess.ratedChargeCurrent, ess.ratedDischargeCurrent,
                ess.charge, ess.health, ess.temperature, ess.voltage,
                ess.ratedVoltage);
#endif
}
