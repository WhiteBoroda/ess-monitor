#ifndef _TYPES_H_
#define _TYPES_H_

#include <stdint.h>

#ifndef VERSION
#define VERSION "v1.1-dev"
#endif

#define CFG_WIFI_STA "wifi.sta"
#define CFG_WIFI_SSID "wifi.ssid"
#define CFG_WIFI_PASS "wifi.pass"
#define CFG_HOSTNAME "hostname"
#define CFG_INVERTER_CHARGE_LIMIT "inverter.charge_limit"
#define CFG_INVERTER_DISCHARGE_LIMIT "inverter.discharge_limit"
#define CFG_MQQTT_ENABLED "mqtt.enabled"
#define CFG_MQQTT_BROKER_IP "mqtt.broker_ip"
#define CFG_MQQTT_PORT "mqtt.port"
#define CFG_MQQTT_USERNAME "mqtt.username"
#define CFG_MQQTT_PASSWORD "mqtt.password"
#define CFG_TG_ENABLED "tg.enabled"
#define CFG_TG_BOT_TOKEN "tg.bot_token"
#define CFG_TG_CHAT_ID "tg.chat_id"
#define CFG_TG_CURRENT_THRESHOLD "tg.amps"
#define CFG_WATCHDOG_ENABLED "watchdog.enabled"
#define CFG_WATCHDOG_TIMEOUT "watchdog.timeout"
#define CFG_SYSLOG_ENABLED "syslog.enabled"
#define CFG_SYSLOG_SERVER "syslog.server"
#define CFG_SYSLOG_PORT "syslog.port"
#define CFG_SYSLOG_LEVEL "syslog.level"
#define CFG_CAN_KEEPALIVE_INTERVAL "can.keepalive_interval"

extern bool needRestart;

// Runtime status (updated from main loop, read from other tasks)
struct RuntimeStatus {
  bool wifiConnected = false;
  char cachedIP[16] = "0.0.0.0";
  char cachedSSID[33] = "";
  int32_t wifiRSSI = 0;
};

typedef struct Config {
  bool wifiSTA = false;
  char wifiSSID[128];
  char wifiPass[128];

  char hostname[32] = "ess-monitor";

  uint8_t chargeLimit = 98;
  uint8_t dishargeLimit = 10;

  bool mqttEnabled = false;
  char mqttBrokerIp[16] = "192.168.0.100";
  uint16_t mqttPort = 1883;
  char mqttUsername[64] = "";
  char mqttPassword[64] = "";

  bool tgEnabled = false;
  char tgBotToken[64];
  char tgChatID[32];
  uint8_t tgCurrentThreshold = 2;

  bool watchdogEnabled = true;    // Watchdog enabled by default to prevent device freezing
  uint8_t watchdogTimeout = 60;   // Watchdog timeout in seconds (default: 60s)

  bool syslogEnabled = false;     // Syslog disabled by default
  char syslogServer[64] = "";     // Syslog server IP or hostname
  uint16_t syslogPort = 514;      // Syslog port (default: 514)
  uint8_t syslogLevel = 6;        // Syslog level (default: INFO=6)

  uint16_t canKeepAliveInterval = 3000;  // CAN keep-alive interval in milliseconds (default: 3000ms = 3 seconds)

} Config;

typedef struct EssStatus {
  int16_t charge;
  int16_t health;
  float voltage;
  float current;
  float ratedVoltage;
  float ratedChargeCurrent;
  float ratedDischargeCurrent;
  float temperature;
  uint8_t bmsWarning;
  uint8_t bmsError;
} EssStatus;

#endif
