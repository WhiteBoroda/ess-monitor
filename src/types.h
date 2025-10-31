#ifndef _TYPES_H_
#define _TYPES_H_

#include <stdint.h>

#ifndef VERSION
#define VERSION "v0.1-dev"
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
#define CFG_RELAY_ENABLED "relay.enabled"
#define CFG_RELAY_PIN "relay.pin"
#define CFG_RELAY_PULSE_MS "relay.pulse_ms"

extern bool needRestart;

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

  bool relayEnabled = false;
  uint8_t relayPin = 13;          // GPIO13 by default (free pin on ESP32)
  uint16_t relayPulseMs = 500;    // 500ms pulse to simulate button press

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
