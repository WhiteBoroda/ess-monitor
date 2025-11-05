#include "web.h"
#include "can.h"
#include "types.h"
#include <GyverPortal.h>
#include <HardwareSerial.h>
#include <Preferences.h>

extern Config Cfg;
extern Preferences Pref;
extern volatile EssStatus Ess;

namespace WEB {

GyverPortal portal;

void begin(uint8_t core, uint8_t priority);
void task(void *pvParameters);
void loop();
void buildPortal();
void onPortalUpdate();

void begin(uint8_t core, uint8_t priority) {
  xTaskCreatePinnedToCore(task, "web_task", 20000, NULL, priority, NULL, core);
}

void task(void *pvParameters) {
  Serial.printf("[WEB] Task running in core %d.\n", (uint32_t)xPortGetCoreID());
  portal.enableOTA();
  portal.attachBuild(buildPortal);
  portal.attach(onPortalUpdate);
  portal.start(Cfg.hostname);

  while (1) {
    loop();
    vTaskDelay(10 / portTICK_PERIOD_MS); // Small delay to prevent task starvation and WDT
  }

  Serial.println("[WEB] Task exited.");
  vTaskDelete(NULL);
}

void loop() { portal.tick(); }

void buildPortal() {
  GP.BUILD_BEGIN(GP_DARK);
  GP.setTimeout(1000);
  GP.ONLINE_CHECK(10000);
  GP.UPDATE("state.charge,state.health,state.voltage,state.current,state."
            "temperature,state.limits,can.keepalive,can.failures,can.lasttime",
            3000);
  GP.PAGE_TITLE(Cfg.hostname);
  GP.NAV_TABS("ESS,WiFi,Telegram,MQTT,Relay,Watchdog,System");
  // Status tab
  GP.NAV_BLOCK_BEGIN();
  GP.GRID_BEGIN();
  M_BLOCK(GP_TAB, "", "Charge", GP.TITLE("-", "state.charge"));
  M_BLOCK(GP_TAB, "", "Load", GP_DEFAULT, GP.TITLE("-", "state.current"));
  GP.GRID_END();
  GP.BLOCK_BEGIN(GP_THIN);
  GP.TABLE_BEGIN();
  GP.TR();
  GP.TD(GP_LEFT);
  GP.PLAIN(F("Voltage"));
  GP.TD(GP_RIGHT);
  GP.BOLD("-", "state.voltage");
  GP.TR();
  GP.TD(GP_LEFT);
  GP.PLAIN(F("Temperature"));
  GP.TD(GP_RIGHT);
  GP.BOLD("-", "state.temperature");
  GP.TR();
  GP.TD(GP_LEFT);
  GP.PLAIN(F("Health"));
  GP.TD(GP_RIGHT);
  GP.BOLD("-", "state.health");
  GP.TR();
  GP.TD(GP_LEFT);
  GP.PLAIN(F("Battery limits"));
  GP.TD(GP_RIGHT);
  GP.BOLD("-", "state.limits");
  GP.TABLE_END();
  GP.BLOCK_END();
  GP.HR();
  GP.SEND(VERSION);
  GP.NAV_BLOCK_END();
  // WiFi settings tab
  GP.NAV_BLOCK_BEGIN();
  GP.FORM_BEGIN("/wifi");
  GP.BOX_BEGIN();
  GP.SWITCH("wifi.sta", Cfg.wifiSTA);
  GP.LABEL("Connect to existing network");
  GP.BOX_END();
  GP.LABEL("SSID");
  GP.TEXT("wifi.ssid", "", Cfg.wifiSSID, "", 128);
  GP.LABEL("Password");
  GP.PASS("wifi.pass", "", Cfg.wifiPass, "", 128);
  GP.BREAK();
  GP.SUBMIT("Save and reboot");
  GP.FORM_END();
  GP.NAV_BLOCK_END();
  // Telegram settings tab
  GP.NAV_BLOCK_BEGIN();
  GP.FORM_BEGIN("/tg");
  GP.BOX_BEGIN();
  GP.SWITCH("tg.enabled", Cfg.tgEnabled);
  GP.LABEL("Enable Telegram notifications");
  GP.BOX_END();
  GP.LABEL("Bot token");
  GP.PASS("tg.bot_token", "", Cfg.tgBotToken, "", sizeof(Cfg.tgBotToken));
  GP.LABEL("Chat ID");
  GP.TEXT("tg.chat_id", "", Cfg.tgChatID, "", sizeof(Cfg.tgChatID));

  GP.LABEL("Current threshold in Amps for alerts");
  char tgCurrentbuf[6];
  itoa(Cfg.tgCurrentThreshold, tgCurrentbuf, 10);
  GP.TEXT("tg.current_threshold", "", tgCurrentbuf, "", sizeof(tgCurrentbuf));
  
  GP.BREAK();
  GP.SUBMIT("Save and reboot");
  GP.FORM_END();
  GP.NAV_BLOCK_END();
  // MQTT
  GP.NAV_BLOCK_BEGIN();
  GP.FORM_BEGIN("/mqtt");
  GP.BOX_BEGIN();
  GP.SWITCH("mqtt.enabled", Cfg.mqttEnabled);
  GP.LABEL("Enable MQTT");
  GP.BOX_END();
  GP.LABEL("Broker IP address");
  GP.TEXT("mqtt.broker_ip", "", Cfg.mqttBrokerIp, "", sizeof(Cfg.mqttBrokerIp));
  GP.LABEL("Port (default: 1883)");
  char mqttPortBuf[6];
  itoa(Cfg.mqttPort, mqttPortBuf, 10);
  GP.TEXT("mqtt.port", "", mqttPortBuf, "", sizeof(mqttPortBuf));
  GP.LABEL("Username (optional)");
  GP.TEXT("mqtt.username", "", Cfg.mqttUsername, "", sizeof(Cfg.mqttUsername));
  GP.LABEL("Password (optional)");
  GP.PASS("mqtt.password", "", Cfg.mqttPassword, "", sizeof(Cfg.mqttPassword));
  GP.BREAK();
  GP.SUBMIT("Save and reboot");
  GP.FORM_END();
  GP.NAV_BLOCK_END();
  // Relay settings tab
  GP.NAV_BLOCK_BEGIN();
  GP.FORM_BEGIN("/relay");
  GP.BOX_BEGIN();
  GP.SWITCH("relay.enabled", Cfg.relayEnabled);
  GP.LABEL("Enable relay control for battery restart");
  GP.BOX_END();
  GP.LABEL("GPIO Pin (ESP32)");
  char relayPinBuf[4];
  itoa(Cfg.relayPin, relayPinBuf, 10);
  GP.TEXT("relay.pin", "", relayPinBuf, "", sizeof(relayPinBuf));
  GP.LABEL("Pulse duration in milliseconds");
  char relayPulseBuf[6];
  itoa(Cfg.relayPulseMs, relayPulseBuf, 10);
  GP.TEXT("relay.pulse_ms", "", relayPulseBuf, "", sizeof(relayPulseBuf));
  GP.HR();
  GP.LABEL("⚠️ WARNING: Connect relay in parallel with BMS power button!");
  GP.LABEL("See RELAY_INSTALLATION.md for detailed instructions.");
  GP.BREAK();
  GP.SUBMIT("Save and reboot");
  GP.FORM_END();
  GP.NAV_BLOCK_END();
  // Watchdog tab
  GP.NAV_BLOCK_BEGIN();
  GP.FORM_BEGIN("/watchdog");
  GP.BOX_BEGIN();
  GP.SWITCH("watchdog.enabled", Cfg.watchdogEnabled);
  GP.LABEL("Enable Hardware Watchdog Timer");
  GP.BOX_END();
  GP.LABEL("Timeout in seconds (10-120)");
  char watchdogTimeoutBuf[4];
  itoa(Cfg.watchdogTimeout, watchdogTimeoutBuf, 10);
  GP.TEXT("watchdog.timeout", "", watchdogTimeoutBuf, "", sizeof(watchdogTimeoutBuf));
  GP.HR();
  GP.LABEL("ℹ️ Watchdog automatically reboots ESP32 if it freezes.");
  GP.LABEL("Recommended: Enabled with 30 second timeout.");
  GP.BREAK();
  GP.SUBMIT("Save and reboot");
  GP.FORM_END();
  GP.NAV_BLOCK_END();
  // System tab
  GP.NAV_BLOCK_BEGIN();
  GP.SYSTEM_INFO();
  GP.HR();
  GP.BLOCK_BEGIN(GP_THIN, "", "CAN Bus Status");
  GP.TABLE_BEGIN();
  GP.TR();
  GP.TD(GP_LEFT);
  GP.PLAIN(F("Keep-alive sent"));
  GP.TD(GP_RIGHT);
  GP.BOLD("-", "can.keepalive");
  GP.TR();
  GP.TD(GP_LEFT);
  GP.PLAIN(F("Send failures"));
  GP.TD(GP_RIGHT);
  GP.BOLD("-", "can.failures");
  GP.TR();
  GP.TD(GP_LEFT);
  GP.PLAIN(F("Last keep-alive"));
  GP.TD(GP_RIGHT);
  GP.BOLD("-", "can.lasttime");
  GP.TABLE_END();
  GP.BLOCK_END();
  GP.HR();
  GP.OTA_FIRMWARE();
  GP.NAV_BLOCK_END();
  GP.BUILD_END();
}

void onPortalUpdate() {
  if (portal.update()) {
    // Get thread-safe copy of battery status
    EssStatus ess = CAN::getEssStatus();

    if (portal.update("state.charge")) {
      portal.answer(String(ess.charge) + "%");
    }
    if (portal.update("state.health")) {
      portal.answer(String(ess.health) + "%");
    }
    if (portal.update("state.current")) {
      portal.answer(String(ess.current, 1) + "A");
    }
    if (portal.update("state.temperature")) {
      portal.answer(String(ess.temperature, 1) + "°C");
    }
    if (portal.update("state.voltage")) {
      portal.answer(String(ess.voltage, 2) + " / " +
                    String(ess.ratedVoltage, 2) + " V");
    }
    if (portal.update("state.limits")) {
      portal.answer(String(ess.ratedChargeCurrent, 1) + " / " +
                    String(ess.ratedDischargeCurrent, 1) + " A");
    }
    if (portal.update("can.keepalive")) {
      portal.answer(String(CAN::getKeepAliveCounter()));
    }
    if (portal.update("can.failures")) {
      portal.answer(String(CAN::getKeepAliveFailures()));
    }
    if (portal.update("can.lasttime")) {
      uint32_t timeSince = CAN::getTimeSinceLastKeepAlive();
      portal.answer(String(timeSince / 1000.0, 1) + " s ago");
    }
  }
  if (portal.form()) {
    // Open Preferences for writing
    Pref.begin("ess");

    if (portal.form("/wifi")) {
      portal.copyBool("wifi.sta", Cfg.wifiSTA);
      portal.copyStr("wifi.ssid", Cfg.wifiSSID, sizeof(Cfg.wifiSSID));
      portal.copyStr("wifi.pass", Cfg.wifiPass, sizeof(Cfg.wifiPass));

      Pref.putBool(CFG_WIFI_STA, Cfg.wifiSTA);
      Pref.putString(CFG_WIFI_SSID, Cfg.wifiSSID);
      Pref.putString(CFG_WIFI_PASS, Cfg.wifiPass);

      Pref.end();
      backToWebRoot();
      needRestart = true;
    } else if (portal.form("/tg")) {
      portal.copyBool("tg.enabled", Cfg.tgEnabled);
      portal.copyStr("tg.bot_token", Cfg.tgBotToken, sizeof(Cfg.tgBotToken));
      portal.copyStr("tg.chat_id", Cfg.tgChatID, sizeof(Cfg.tgChatID));

      int thr = (int)Cfg.tgCurrentThreshold; 
      portal.copyInt("tg.current_threshold", thr);
      if (thr < 0)   thr = 0;
      if (thr > 100) thr = 100;
      Cfg.tgCurrentThreshold = (uint8_t)thr;

      Pref.putBool(CFG_TG_ENABLED, Cfg.tgEnabled);
      Pref.putString(CFG_TG_BOT_TOKEN, Cfg.tgBotToken);
      Pref.putString(CFG_TG_CHAT_ID, Cfg.tgChatID);
      Pref.putUChar (CFG_TG_CURRENT_THRESHOLD, Cfg.tgCurrentThreshold);

      Pref.end();
      backToWebRoot();
      needRestart = true;
    } else if (portal.form("/mqtt")) {
      portal.copyBool("mqtt.enabled", Cfg.mqttEnabled);
      portal.copyStr("mqtt.broker_ip", Cfg.mqttBrokerIp,
                     sizeof(Cfg.mqttBrokerIp));

      int port = (int)Cfg.mqttPort;
      portal.copyInt("mqtt.port", port);
      if (port < 1) port = 1;
      if (port > 65535) port = 65535;
      Cfg.mqttPort = (uint16_t)port;

      portal.copyStr("mqtt.username", Cfg.mqttUsername,
                     sizeof(Cfg.mqttUsername));
      portal.copyStr("mqtt.password", Cfg.mqttPassword,
                     sizeof(Cfg.mqttPassword));

      Pref.putBool(CFG_MQQTT_ENABLED, Cfg.mqttEnabled);
      Pref.putString(CFG_MQQTT_BROKER_IP, Cfg.mqttBrokerIp);
      Pref.putUShort(CFG_MQQTT_PORT, Cfg.mqttPort);
      Pref.putString(CFG_MQQTT_USERNAME, Cfg.mqttUsername);
      Pref.putString(CFG_MQQTT_PASSWORD, Cfg.mqttPassword);

      Pref.end();
      backToWebRoot();
      needRestart = true;
    } else if (portal.form("/relay")) {
      portal.copyBool("relay.enabled", Cfg.relayEnabled);

      int pin = (int)Cfg.relayPin;
      portal.copyInt("relay.pin", pin);
      if (pin < 0) pin = 0;
      if (pin > 39) pin = 39; // ESP32 max GPIO
      Cfg.relayPin = (uint8_t)pin;

      int pulse = (int)Cfg.relayPulseMs;
      portal.copyInt("relay.pulse_ms", pulse);
      if (pulse < 100) pulse = 100;   // Min 100ms
      if (pulse > 5000) pulse = 5000; // Max 5s
      Cfg.relayPulseMs = (uint16_t)pulse;

      Pref.putBool(CFG_RELAY_ENABLED, Cfg.relayEnabled);
      Pref.putUChar(CFG_RELAY_PIN, Cfg.relayPin);
      Pref.putUShort(CFG_RELAY_PULSE_MS, Cfg.relayPulseMs);

      Pref.end();
      backToWebRoot();
      needRestart = true;
    } else if (portal.form("/watchdog")) {
      portal.copyBool("watchdog.enabled", Cfg.watchdogEnabled);

      int timeout = (int)Cfg.watchdogTimeout;
      portal.copyInt("watchdog.timeout", timeout);
      if (timeout < 10) timeout = 10;     // Min 10 seconds
      if (timeout > 120) timeout = 120;   // Max 120 seconds
      Cfg.watchdogTimeout = (uint8_t)timeout;

      Pref.putBool(CFG_WATCHDOG_ENABLED, Cfg.watchdogEnabled);
      Pref.putUChar(CFG_WATCHDOG_TIMEOUT, Cfg.watchdogTimeout);

      Pref.end();
      backToWebRoot();
      needRestart = true;
    }
  }
}

void backToWebRoot() {
  portal.answer(F("<meta http-equiv='refresh' content='0; url=/' />"));
}

} // namespace WEB
