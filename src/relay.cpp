#include "relay.h"
#include "types.h"
#include <Arduino.h>
#include <HardwareSerial.h>

extern Config Cfg;

namespace RELAY {

void begin() {
  if (!Cfg.relayEnabled) {
    Serial.println("[RELAY] Relay control disabled in config.");
    return;
  }

  pinMode(Cfg.relayPin, OUTPUT);
  digitalWrite(Cfg.relayPin, LOW);
  Serial.printf("[RELAY] Initialized on GPIO%d, pulse duration: %dms\n",
                Cfg.relayPin, Cfg.relayPulseMs);
}

void triggerPulse() {
  if (!Cfg.relayEnabled) {
    Serial.println("[RELAY] Relay control disabled, ignoring trigger request.");
    return;
  }

  Serial.printf("[RELAY] Triggering relay pulse on GPIO%d for %dms\n",
                Cfg.relayPin, Cfg.relayPulseMs);

  // Activate relay (simulate button press)
  digitalWrite(Cfg.relayPin, HIGH);
  delay(Cfg.relayPulseMs);
  digitalWrite(Cfg.relayPin, LOW);

  Serial.println("[RELAY] Relay pulse completed.");
}

bool isEnabled() {
  return Cfg.relayEnabled;
}

} // namespace RELAY
