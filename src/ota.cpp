#include "ota.h"
#include "types.h"
#include <ArduinoOTA.h>
#include <WiFi.h>

extern Config Cfg;

namespace OTA {

void begin() {
  // Set OTA hostname
  ArduinoOTA.setHostname(Cfg.hostname);

  // Optional: Set OTA password for security
  // ArduinoOTA.setPassword("admin");

  // OTA callbacks
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }
    Serial.println("[OTA] Start updating " + type);
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\n[OTA] Update complete!");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    static unsigned int lastPercent = 0;
    unsigned int percent = (progress / (total / 100));
    if (percent != lastPercent && percent % 10 == 0) {
      Serial.printf("[OTA] Progress: %u%%\n", percent);
      lastPercent = percent;
    }
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("[OTA] Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });

  ArduinoOTA.begin();
  Serial.println("[OTA] OTA Update ready");
  Serial.printf("[OTA] Hostname: %s\n", Cfg.hostname);
  Serial.printf("[OTA] IP: %s\n", WiFi.localIP().toString().c_str());
}

void handle() {
  ArduinoOTA.handle();
}

} // namespace OTA
