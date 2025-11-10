#include "hass.h"
#include "can.h"
#include <esp_task_wdt.h>

extern Config Cfg;
extern volatile EssStatus Ess;

namespace HASS {

WiFiClient client;
HADevice device;
HAMqtt mqtt(client, device);
HASensorNumber chargeSensor("charge");
HASensorNumber healthSensor("health");
HASensorNumber voltageSensor("voltage", HASensorNumber::PrecisionP2);
HASensorNumber ratedVoltageSensor("rated_voltage", HASensorNumber::PrecisionP2);
HASensorNumber currentSensor("current", HASensorNumber::PrecisionP1);
HASensorNumber ratedChargeCurrentSensor("rated_charge_current",
                                        HASensorNumber::PrecisionP1);
HASensorNumber ratedDischargeCurrentSensor("rated_discharge_current",
                                           HASensorNumber::PrecisionP1);
HASensorNumber temperatureSensor("temperature", HASensorNumber::PrecisionP1);
HASensor bmsWarningSensor("bms_warning");
HASensor bmsErrorSensor("bms_error");

void begin(uint8_t core, uint8_t priority);
void task(void *pvParameters);
void loop();

void begin(uint8_t core, uint8_t priority) {
  xTaskCreatePinnedToCore(task, "hass_task", 20000, NULL, priority, NULL, core);
}

void task(void *pvParameters) {
  Serial.printf("[HASS] Task running in core %d.\n",
                (uint32_t)xPortGetCoreID());

  uint8_t mac[6];
  WiFi.macAddress(mac);
  device.setUniqueId(mac, sizeof(mac));
  device.enableExtendedUniqueIds();
  device.setName("ESS Monitor");
  device.setModel("ess-monitor");
  device.setManufacturer("Bimba Perdoling.");
  device.setSoftwareVersion(VERSION);
  char url[24];
  String("http://" + WiFi.localIP().toString()).toCharArray(url, sizeof(url));
  device.setConfigurationUrl(url);
  device.enableSharedAvailability();
  device.enableLastWill();

  chargeSensor.setIcon("mdi:battery-high");
  chargeSensor.setName("Charge");
  chargeSensor.setDeviceClass("battery");
  chargeSensor.setUnitOfMeasurement("%");

  healthSensor.setIcon("mdi:heart");
  healthSensor.setName("Health");
  healthSensor.setUnitOfMeasurement("%");

  voltageSensor.setIcon("mdi:flash-triangle-outline");
  voltageSensor.setName("Voltage");
  voltageSensor.setDeviceClass("voltage");
  voltageSensor.setUnitOfMeasurement("V");

  ratedVoltageSensor.setIcon("mdi:flash-triangle");
  ratedVoltageSensor.setName("Rated voltage");
  ratedVoltageSensor.setDeviceClass("voltage");
  ratedVoltageSensor.setUnitOfMeasurement("V");

  currentSensor.setIcon("mdi:current-dc");
  currentSensor.setName("Current");
  currentSensor.setDeviceClass("current");
  currentSensor.setUnitOfMeasurement("A");

  ratedChargeCurrentSensor.setIcon("mdi:current-dc");
  ratedChargeCurrentSensor.setName("Rated charge current");
  ratedChargeCurrentSensor.setDeviceClass("current");
  ratedChargeCurrentSensor.setUnitOfMeasurement("A");

  ratedDischargeCurrentSensor.setIcon("mdi:current-dc");
  ratedDischargeCurrentSensor.setName("Rated discharge current");
  ratedDischargeCurrentSensor.setDeviceClass("current");
  ratedDischargeCurrentSensor.setUnitOfMeasurement("A");

  temperatureSensor.setIcon("mdi:thermometer");
  temperatureSensor.setName("Temperature");
  temperatureSensor.setDeviceClass("temperature");
  temperatureSensor.setUnitOfMeasurement("°C");

  bmsWarningSensor.setIcon("mdi:alert");
  bmsWarningSensor.setDeviceClass("enum");
  bmsWarningSensor.setName("BMS warning");

  bmsErrorSensor.setIcon("mdi:alert-octagon");
  bmsErrorSensor.setDeviceClass("enum");
  bmsErrorSensor.setName("BMS error");

  IPAddress ip;
  ip.fromString(Cfg.mqttBrokerIp);

  // Use authentication if username and password are provided
  if (strlen(Cfg.mqttUsername) > 0 && strlen(Cfg.mqttPassword) > 0) {
    Serial.printf("[HASS] Connecting to MQTT broker %s:%d with authentication (user: %s)\n",
                  Cfg.mqttBrokerIp, Cfg.mqttPort, Cfg.mqttUsername);
    mqtt.begin(ip, Cfg.mqttPort, Cfg.mqttUsername, Cfg.mqttPassword);
  } else {
    Serial.printf("[HASS] Connecting to MQTT broker %s:%d without authentication\n",
                  Cfg.mqttBrokerIp, Cfg.mqttPort);
    mqtt.begin(ip, Cfg.mqttPort);
  }

  Serial.println("[HASS] Waiting for MQTT connection...");

  // Wait for MQTT connection to establish
  uint8_t connectionAttempts = 0;
  while (!mqtt.isConnected() && connectionAttempts < 30) {
    mqtt.loop();
    vTaskDelay(500 / portTICK_PERIOD_MS);
    connectionAttempts++;
    if (connectionAttempts % 5 == 0) {
      Serial.printf("[HASS] Still connecting... attempt %d/30\n", connectionAttempts);
    }
  }

  if (mqtt.isConnected()) {
    Serial.println("[HASS] ✓ MQTT connected successfully!");
  } else {
    Serial.println("[HASS] ⚠️ WARNING: Could not connect to MQTT broker!");
  }

  // Start MQTT loop immediately to publish discovery messages
  Serial.println("[HASS] Starting MQTT loop and publishing discovery...");
  Serial.printf("[HASS] Device info: Name='%s', Model='%s', MAC=%02X:%02X:%02X:%02X:%02X:%02X\n",
                "ESS Monitor", "ess-monitor", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.printf("[HASS] MQTT will publish to discovery prefix: homeassistant\n");
  Serial.printf("[HASS] Number of entities to publish: 10 sensors\n");

  // Run initial loops to publish discovery and establish connection
  for (int i = 0; i < 100; i++) {
    mqtt.loop();
    vTaskDelay(200 / portTICK_PERIOD_MS);

    // CRITICAL FIX: Reset watchdog during long initialization (20 seconds total)
    if (Cfg.watchdogEnabled) {
      esp_task_wdt_reset();
    }

    if (i % 20 == 0) {
      Serial.printf("[HASS] Discovery progress: %d/100 loops... (connected: %s)\n",
                    i, mqtt.isConnected() ? "YES" : "NO");
    }
  }

  Serial.println("[HASS] ✓ Discovery published, entering main loop.");

  while (1) {
    loop();

    // Reset watchdog timer to prevent device reboot
    if (Cfg.watchdogEnabled) {
      esp_task_wdt_reset();
    }

    vTaskDelay(100 / portTICK_PERIOD_MS); // Small delay to prevent task starvation
  }

  Serial.println("[HASS] Task exited.");
  vTaskDelete(NULL);
};

void loop() {
  static uint32_t previousMillis = 0;
  static uint32_t statusCheckMillis = 0;
  static bool firstRun = true;
  uint32_t currentMillis = millis();

  // Check MQTT connection status every 30 seconds
  if (currentMillis - statusCheckMillis >= 1000 * 30) {
    statusCheckMillis = currentMillis;
    Serial.printf("[HASS] MQTT connection status: %s\n",
                  mqtt.isConnected() ? "CONNECTED" : "DISCONNECTED");
  }

  // Publish initial values immediately, then every 5 seconds
  if (firstRun || (currentMillis - previousMillis >= 1000 * 5)) {
    previousMillis = currentMillis;

#ifdef DEBUG
    Serial.printf("[HASS] Publishing sensor updates (connected: %s)...\n",
                  mqtt.isConnected() ? "YES" : "NO");
#endif

    if (firstRun) {
      Serial.println("[HASS] First sensor value publication...");
      firstRun = false;
    }

    // Get thread-safe copy of battery status
    EssStatus ess = CAN::getEssStatus();

    char buf[4];

    chargeSensor.setValue(ess.charge);
    healthSensor.setValue(ess.health);
    voltageSensor.setValue(ess.voltage);
    ratedVoltageSensor.setValue(ess.ratedVoltage);
    currentSensor.setValue(ess.current);
    ratedChargeCurrentSensor.setValue(ess.ratedChargeCurrent);
    ratedDischargeCurrentSensor.setValue(ess.ratedDischargeCurrent);
    temperatureSensor.setValue(ess.temperature);
    sprintf(buf, "%d", ess.bmsWarning);
    bmsWarningSensor.setValue(buf);
    sprintf(buf, "%d", ess.bmsError);
    bmsErrorSensor.setValue(buf);

#ifdef DEBUG
    Serial.printf("[HASS] Published: SOC=%d%%, SOH=%d%%, V=%.2f, I=%.1f, T=%.1f\n",
                  ess.charge, ess.health, ess.voltage, ess.current, ess.temperature);
#endif
  }

  mqtt.loop();
}

} // namespace HASS
