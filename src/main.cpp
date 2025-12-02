#include "can.h"
#include "hass.h"
#include "lcd.h"
#include "logger.h"
#include "ota.h"
#include "tg.h"
#include "types.h"
#include "web.h"
#include "runtime_cache.h"
#include "wifi_manager.h"
#include <Arduino.h>
#include <Preferences.h>
#include <esp_system.h>
#include <esp_task_wdt.h>

static const char *resetReasonToString(esp_reset_reason_t reason) {
  switch (reason) {
  case ESP_RST_UNKNOWN:
    return "Unknown";
  case ESP_RST_POWERON:
    return "Power-on";
  case ESP_RST_EXT:
    return "External pin";
  case ESP_RST_SW:
    return "Software";
  case ESP_RST_PANIC:
    return "Software panic";
  case ESP_RST_INT_WDT:
    return "Interrupt watchdog";
  case ESP_RST_TASK_WDT:
    return "Task watchdog";
  case ESP_RST_WDT:
    return "Other watchdog";
  case ESP_RST_DEEPSLEEP:
    return "Deep sleep";
  case ESP_RST_BROWNOUT:
    return "Brownout";
  case ESP_RST_SDIO:
    return "SDIO";
#ifdef ESP_RST_USB
  case ESP_RST_USB:
    return "USB";
#endif
#ifdef ESP_RST_JTAG
  case ESP_RST_JTAG:
    return "JTAG";
#endif
#ifdef ESP_RST_TIMEWDT
  case ESP_RST_TIMEWDT:
    return "Time watchdog";
#endif
#ifdef ESP_RST_RTCWDT
  case ESP_RST_RTCWDT:
    return "RTC watchdog";
#endif
  default:
    return "Other";
  }
}

Preferences Pref;
Config Cfg;
volatile EssStatus Ess;

void initConfig();
void logBatteryState();

bool needRestart = false;

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n========== ESS Monitor Starting ==========");

  esp_reset_reason_t resetReason = esp_reset_reason();
  const char *resetReasonStr = resetReasonToString(resetReason);
  Serial.printf("[MAIN] Previous reset reason: %s (code=%d)\n", resetReasonStr,
                static_cast<int>(resetReason));
  if (resetReason == ESP_RST_TASK_WDT || resetReason == ESP_RST_WDT
#ifdef ESP_RST_INT_WDT
      || resetReason == ESP_RST_INT_WDT
#endif
#ifdef ESP_RST_TIMEWDT
      || resetReason == ESP_RST_TIMEWDT
#endif
#ifdef ESP_RST_RTCWDT
      || resetReason == ESP_RST_RTCWDT
#endif
  ) {
    Serial.println(
        "[MAIN] ⚠ Watchdog reset detected — device rebooted by watchdog.");
  }

  // Load configuration from flash
  initConfig();

  // Initialize LCD display first so user sees something
  LCD::begin(1, 1);

  // Initialize CAN bus immediately to start reading battery data
  CAN::begin(1, 1);

  // Initialize WiFi with captive portal (this can take up to 180s)
  bool wifiConnected = WiFiMgr::begin();

  // Initialize OTA updates (must be after WiFi)
  if (wifiConnected) {
    OTA::begin();
  }

  // Initialize web server first (to setup WebSerial for logging)
  WEB::begin();

  // Initialize Logger AFTER WebSerial is ready
  Logger::begin();

  // Initialize MQTT if WiFi connected and enabled
  if (wifiConnected && Cfg.mqttEnabled) {
    HASS::begin(1, 1);
  }

  // Initialize Telegram if WiFi connected and enabled
  if (wifiConnected && Cfg.tgEnabled) {
    TG::begin(1, 1);
  }

  // Initialize Hardware Watchdog Timer
  if (Cfg.watchdogEnabled) {
    Serial.printf("[MAIN] Enabling Hardware Watchdog Timer: %d seconds\n", Cfg.watchdogTimeout);
    esp_task_wdt_init(Cfg.watchdogTimeout, true); // timeout in seconds, panic on timeout
    esp_task_wdt_add(NULL); // Add current task (loop task) to WDT
    Serial.println("[MAIN] ✓ Watchdog Timer enabled");
  } else {
    Serial.println("[MAIN] Watchdog Timer disabled by configuration");
  }
}

void loop() {
  static uint32_t previousMillis;
  uint32_t currentMillis = millis();

  // Handle OTA updates
  OTA::handle();
  
  // Handle Web Server cleanup
  WEB::loop();

  // Update runtime status (WiFi status for tasks running on other core)
  RuntimeCache::updateFromWiFi();

  // Reset Watchdog Timer to prevent reboot
  if (Cfg.watchdogEnabled) {
    esp_task_wdt_reset();
  }

  // Soft restart if requested (with delay to allow sending response)
  if (needRestart) {
    static uint32_t restartTime = 0;
    if (restartTime == 0) {
      restartTime = millis();
      Serial.println("[MAIN] Restart requested. Rebooting in 1s...");
    }
    
    if (millis() - restartTime > 1000) {
      Serial.println("[MAIN] Rebooting now!");
      delay(100); // Short delay for Serial flush
      ESP.restart();
    }
  }

  // Every 3 seconds: update WebSocket data and log battery state
  if (currentMillis - previousMillis >= 3000) {
    previousMillis = currentMillis;

    // Update live data for web clients
    WEB::updateLiveData();

    // Log battery state (if DEBUG defined)
    logBatteryState();
  }
}

void initConfig() {
  Pref.begin("ess");

  Cfg.wifiSTA = Pref.getBool(CFG_WIFI_STA, Cfg.wifiSTA);
  Pref.getString(CFG_WIFI_SSID, Cfg.wifiSSID, sizeof(Cfg.wifiSSID));
  Pref.getString(CFG_WIFI_PASS, Cfg.wifiPass, sizeof(Cfg.wifiPass));

  Pref.getString(CFG_HOSTNAME, Cfg.hostname, sizeof(Cfg.hostname));

  // Auto-generate unique hostname if default or empty
  if (strlen(Cfg.hostname) == 0 || strcmp(Cfg.hostname, "ess-monitor") == 0) {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(Cfg.hostname, sizeof(Cfg.hostname), "ess-mon-%02x%02x", mac[4], mac[5]);
    Serial.printf("[MAIN] Generated unique hostname: %s\n", Cfg.hostname);
  }

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
