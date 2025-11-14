#include "can.h"
#include "hass.h"
#include "lcd.h"
#include "logger.h"
#include "ota.h"
#include "tg.h"
#include "types.h"
#include "web.h"
#include "wifi_manager.h"
#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_system.h>
#include <esp_task_wdt.h>
#include <string.h>

Preferences Pref;
Config Cfg;
volatile EssStatus Ess;
RuntimeStatus Runtime;
ResetStatus Reset;

void initConfig();
void logBatteryState();
void updateRuntimeStatus();
void captureResetInfo();
void logResetInfo();
bool needRestart = false;

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n========== ESS Monitor Starting ==========");

  captureResetInfo();
  Serial.printf("[RESET] Last reset reason: %s (%d)\n", Reset.reasonLabel,
                Reset.reasonCode);
  if (Reset.detail[0] != '\0') {
    Serial.printf("[RESET] %s\n", Reset.detail);
  }

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
  logResetInfo();

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

  // Update runtime status (WiFi status for tasks running on other core)
  updateRuntimeStatus();

  // Reset Watchdog Timer to prevent reboot
  if (Cfg.watchdogEnabled) {
    esp_task_wdt_reset();
  }

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

namespace {
const char *resetReasonToString(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_UNKNOWN:
      return "Unknown";
    case ESP_RST_POWERON:
      return "Power-on";
    case ESP_RST_EXT:
      return "External";
    case ESP_RST_SW:
      return "Software";
    case ESP_RST_PANIC:
      return "Panic";
    case ESP_RST_INT_WDT:
      return "Interrupt Watchdog";
    case ESP_RST_TASK_WDT:
      return "Task Watchdog";
    case ESP_RST_WDT:
      return "Other Watchdog";
    case ESP_RST_DEEPSLEEP:
      return "Deep-sleep";
    case ESP_RST_BROWNOUT:
      return "Brownout";
    case ESP_RST_SDIO:
      return "SDIO";
#ifdef ESP_RST_USB
    case ESP_RST_USB:
      return "USB";
#endif
#ifdef ESP_RST_RTCWDT
    case ESP_RST_RTCWDT:
      return "RTC Watchdog";
#endif
#ifdef ESP_RST_GLITCH
    case ESP_RST_GLITCH:
      return "Glitch";
#endif
#ifdef ESP_RST_CHIP
    case ESP_RST_CHIP:
      return "Chip";
#endif
#ifdef ESP_RST_CORE
    case ESP_RST_CORE:
      return "Core";
#endif
#ifdef ESP_RST_APPCPU
    case ESP_RST_APPCPU:
      return "App CPU";
#endif
#ifdef ESP_RST_BBPLL
    case ESP_RST_BBPLL:
      return "BBPLL";
#endif
#ifdef ESP_RST_JTAG
    case ESP_RST_JTAG:
      return "JTAG";
#endif
    default:
      return "Unlisted";
  }
}

bool isWatchdogReason(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:
#ifdef ESP_RST_RTCWDT
    case ESP_RST_RTCWDT:
#endif
      return true;
    default:
      return false;
  }
}

const char *detailHintFor(esp_reset_reason_t reason) {
  if (isWatchdogReason(reason)) {
    return "Watchdog reset detected. Check for blocking tasks or deadlocks.";
  }

  switch (reason) {
    case ESP_RST_PANIC:
      return "CPU panic reset detected. Inspect crash logs for root cause.";
    case ESP_RST_BROWNOUT:
      return "Brownout detected. Verify power supply stability.";
    default:
      return "";
  }
}

Logger::Level levelForReset(esp_reset_reason_t reason) {
  if (reason == ESP_RST_PANIC) {
    return Logger::LEVEL_ERR;
  }
  if (isWatchdogReason(reason) || reason == ESP_RST_BROWNOUT) {
    return Logger::LEVEL_WARNING;
  }
  return Logger::LEVEL_INFO;
}
} // namespace

void updateRuntimeStatus() {
  Runtime.wifiConnected = (WiFi.status() == WL_CONNECTED);

  if (Runtime.wifiConnected) {
    strlcpy(Runtime.cachedIP, WiFi.localIP().toString().c_str(),
            sizeof(Runtime.cachedIP));
  } else {
    strlcpy(Runtime.cachedIP, "0.0.0.0", sizeof(Runtime.cachedIP));
  }
}

void captureResetInfo() {
  esp_reset_reason_t reason = esp_reset_reason();
  Reset.reasonCode = static_cast<int>(reason);
  Reset.watchdog = isWatchdogReason(reason);
  strlcpy(Reset.reasonLabel, resetReasonToString(reason),
          sizeof(Reset.reasonLabel));
  strlcpy(Reset.detail, detailHintFor(reason), sizeof(Reset.detail));
}

void logResetInfo() {
  Logger::Level level =
      levelForReset(static_cast<esp_reset_reason_t>(Reset.reasonCode));

  switch (level) {
    case Logger::LEVEL_ERR:
      LOG_E("RESET", "Last reset reason: %s (%d)", Reset.reasonLabel,
            Reset.reasonCode);
      break;
    case Logger::LEVEL_WARNING:
      LOG_W("RESET", "Last reset reason: %s (%d)", Reset.reasonLabel,
            Reset.reasonCode);
      break;
    default:
      LOG_I("RESET", "Last reset reason: %s (%d)", Reset.reasonLabel,
            Reset.reasonCode);
      break;
  }

  if (Reset.detail[0] != '\0') {
    if (level == Logger::LEVEL_ERR) {
      LOG_E("RESET", "%s", Reset.detail);
    } else if (level == Logger::LEVEL_WARNING) {
      LOG_W("RESET", "%s", Reset.detail);
    } else {
      LOG_I("RESET", "%s", Reset.detail);
    }
  }
}

