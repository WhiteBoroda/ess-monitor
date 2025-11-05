#include "logger.h"
#include "types.h"
#include <Syslog.h>
#include <WiFi.h>
#include <WiFiUdp.h>

extern Config Cfg;

namespace Logger {

WiFiUDP udpClient;
Syslog syslog(udpClient, SYSLOG_PROTO_IETF);
bool enabled = false;
Level currentLevel = LEVEL_INFO; // Default log level

void begin() {
  // Set log level from config
  currentLevel = (Level)Cfg.syslogLevel;

  if (Cfg.syslogEnabled && strlen(Cfg.syslogServer) > 0) {
    // Configure syslog
    syslog.server(Cfg.syslogServer, Cfg.syslogPort);
    syslog.deviceHostname(Cfg.hostname);
    syslog.appName("ess-monitor");
    syslog.defaultPriority(6); // LOG_INFO

    enabled = true;

    Serial.printf("[LOGGER] Syslog enabled: %s:%d, level: %d\n",
                  Cfg.syslogServer, Cfg.syslogPort, currentLevel);

    // Send startup message
    syslog.logf(6, "ESS Monitor started, version: %s, log level: %d", VERSION, currentLevel);
  } else {
    enabled = false;
    Serial.printf("[LOGGER] Syslog disabled, log level: %d\n", currentLevel);
  }
}

void setEnabled(bool en) {
  enabled = en;
}

bool isEnabled() {
  return enabled && Cfg.syslogEnabled;
}

void setLevel(Level level) {
  currentLevel = level;
  Serial.printf("[LOGGER] Log level changed to: %d\n", level);
}

Level getLevel() {
  return currentLevel;
}

void log(Level level, const char* tag, const char* format, ...) {
  char buffer[256];
  va_list args;

  // Format the message
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  // Always write to Serial
  Serial.printf("[%s] %s\n", tag, buffer);

  // Send to syslog if enabled and level is <= configured level
  // Lower number = higher priority (EMERG=0, DEBUG=7)
  if (isEnabled() && WiFi.status() == WL_CONNECTED && level <= currentLevel) {
    char syslogMsg[384];
    snprintf(syslogMsg, sizeof(syslogMsg), "[%s] %s", tag, buffer);
    syslog.log((uint16_t)level, syslogMsg);
  }
}

void emergency(const char* tag, const char* format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  log(LEVEL_EMERG, tag, buffer);
}

void alert(const char* tag, const char* format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  log(LEVEL_ALERT, tag, buffer);
}

void critical(const char* tag, const char* format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  log(LEVEL_CRIT, tag, buffer);
}

void error(const char* tag, const char* format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  log(LEVEL_ERR, tag, buffer);
}

void warning(const char* tag, const char* format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  log(LEVEL_WARNING, tag, buffer);
}

void notice(const char* tag, const char* format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  log(LEVEL_NOTICE, tag, buffer);
}

void info(const char* tag, const char* format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  log(LEVEL_INFO, tag, buffer);
}

void debug(const char* tag, const char* format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  log(LEVEL_DEBUG, tag, buffer);
}

} // namespace Logger
