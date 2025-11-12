#include "can.h"
#include "hass.h"
#include "lcd.h"
#include "logger.h"
#include "ota.h"
#include "tg.h"
#include "types.h"
#include "watchdog.h"
#include "web.h"
#include "wifi_manager.h"
#include <Arduino.h>
#include <Preferences.h>

Preferences Pref;
Config Cfg;
volatile EssStatus Ess;
RuntimeStatus Runtime;

void initConfig();
void logBatteryState();
void updateRuntimeStatus();

bool needRestart = false;

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n========== ESS Monitor Starting ==========");

  // Load configuration from flash
  initConfig();

  // Initialize WiFi with captive portal
  bool wifiConnected = WiFiMgr::begin();

  // Initialize OTA updates (must be after WiFi)
  if (wifiConnected) {
    OTA::begin();
  }

  // Initialize web server first (to setup WebSerial for logging)
  WEB::begin();

  // Initialize Logger AFTER WebSerial is ready
  Logger::begin();

  // Initialize CAN bus (logs will go to WebSerial now)
  CAN::begin(1, 1);

  // Initialize LCD display
  LCD::begin(1, 1);

  // Initialize MQTT if WiFi connected and enabled
  if (wifiConnected && Cfg.mqttEnabled) {
    HASS::begin(1, 1);
  }

  // Initialize Telegram if WiFi connected and enabled
  if (wifiConnected && Cfg.tgEnabled) {
    TG::begin(1, 1);
  }

  // Initialize inter-core watchdog on Core 1 to monitor Core 0
  // CRITICAL: Watchdog runs on Core 1, so if Core 0 freezes, watchdog will detect it!
  WATCHDOG::begin(1, 2); // Core 1, high priority
}

void loop() {
  static uint32_t previousMillis;
  uint32_t currentMillis = millis();

  // Send heartbeat to watchdog task on Core 1
  // CRITICAL: This tells Core 1 that Core 0 is still alive!
  // If this stops being called, Core 1 watchdog will restart ESP32
  WATCHDOG::heartbeat();

  // Handle OTA updates
  OTA::handle();

  // Update runtime status (WiFi status for LCD task)
  updateRuntimeStatus();

  // Every 3 seconds: update WebSocket data and log battery state
  if (currentMillis - previousMillis >= 3000) {
    previousMillis = currentMillis;

    // Update live data for web clients
    WEB::updateLiveData();

    // Log battery state (if DEBUG defined)
    logBatteryState();

    // Soft restart if requested
    if (needRestart) {
      Serial.println("[MAIN] Restarting device...");
      ESP.restart();
    }
  }
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

  Cfg.canKeepAliveInterval = Pref.getUShort(CFG_CAN_KEEPALIVE_INTERVAL, Cfg.canKeepAliveInterval);

  Pref.end();
}

void logBatteryState() {
#ifdef DEBUG
  // Get thread-safe copy of battery status
  EssStatus ess = CAN::getEssStatus();

  // Store previous values
  static int prevCharge = -1;
  static int prevHealth = -1;
  static float prevCurrent = -999.0;
  static float prevVoltage = -1.0;
  static float prevTemperature = -999.0;

  // Log only if significant changes occurred (to reduce log spam)
  bool changed = (ess.charge != prevCharge) ||
                 (ess.health != prevHealth) ||
                 (abs(ess.current - prevCurrent) > 0.5) ||
                 (abs(ess.voltage - prevVoltage) > 0.1) ||
                 (abs(ess.temperature - prevTemperature) > 0.5);

  if (changed) {
    // Use Logger to output to both Serial and WebSerial
    LOG_D("MAIN", "Load %.1f | RC %.1f/%.1f | SOC %d%% | SOH %d%% | T %.1f°C | V %.2f/%.2f",
          ess.current, ess.ratedChargeCurrent, ess.ratedDischargeCurrent,
          ess.charge, ess.health, ess.temperature, ess.voltage,
          ess.ratedVoltage);

    // Update previous values
    prevCharge = ess.charge;
    prevHealth = ess.health;
    prevCurrent = ess.current;
    prevVoltage = ess.voltage;
    prevTemperature = ess.temperature;
  }
#endif
}

void updateRuntimeStatus() {
  // Update WiFi status from Core 0 (main loop) for use in other tasks (Core 1)
  // WiFi.status() is not thread-safe, so we cache the result here
  Runtime.wifiConnected = (WiFi.status() == WL_CONNECTED);

  if (Runtime.wifiConnected) {
    // Cache IP address to avoid blocking calls from other tasks
    strlcpy(Runtime.cachedIP, WiFi.localIP().toString().c_str(), sizeof(Runtime.cachedIP));
  } else {
    strlcpy(Runtime.cachedIP, "0.0.0.0", sizeof(Runtime.cachedIP));
  }
}
