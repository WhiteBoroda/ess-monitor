#include "relay.h"
#include "types.h"
#include "logger.h"
#include <Arduino.h>
#include <HardwareSerial.h>

extern Config Cfg;

namespace RELAY {

void begin() {
  if (!Cfg.relayEnabled) {
    LOG_I("RELAY", "Relay control disabled in config");
    return;
  }

  pinMode(Cfg.relayPin, OUTPUT);
  digitalWrite(Cfg.relayPin, LOW);
  LOG_I("RELAY", "Initialized on GPIO%d, pulse duration: %dms",
        Cfg.relayPin, Cfg.relayPulseMs);
}

void triggerPulse() {
  if (!Cfg.relayEnabled) {
    LOG_W("RELAY", "Relay control disabled, ignoring trigger request");
    return;
  }

  LOG_I("RELAY", "Triggering relay pulse on GPIO%d for %dms",
        Cfg.relayPin, Cfg.relayPulseMs);

  // Activate relay (simulate button press)
  digitalWrite(Cfg.relayPin, HIGH);
  delay(Cfg.relayPulseMs);
  digitalWrite(Cfg.relayPin, LOW);

  LOG_I("RELAY", "Relay pulse completed");
}

bool isEnabled() {
  return Cfg.relayEnabled;
}

} // namespace RELAY
