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

void begin() {
  if (Cfg.syslogEnabled && strlen(Cfg.syslogServer) > 0) {
    // Configure syslog
    syslog.server(Cfg.syslogServer, Cfg.syslogPort);
    syslog.deviceHostname(Cfg.hostname);
    syslog.appName("ess-monitor");
    syslog.defaultPriority(LOG_INFO);

    enabled = true;

    Serial.printf("[LOGGER] Syslog enabled: %s:%d\n",
                  Cfg.syslogServer, Cfg.syslogPort);

    // Send startup message
    syslog.logf(LOG_INFO, "ESS Monitor started, version: %s", VERSION);
  } else {
    enabled = false;
    Serial.println("[LOGGER] Syslog disabled");
  }
}

void setEnabled(bool en) {
  enabled = en;
}

bool isEnabled() {
  return enabled && Cfg.syslogEnabled;
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

  // Send to syslog if enabled
  if (isEnabled() && WiFi.status() == WL_CONNECTED) {
    char syslogMsg[384];
    snprintf(syslogMsg, sizeof(syslogMsg), "[%s] %s", tag, buffer);
    syslog.log(level, syslogMsg);
  }
}

void emergency(const char* tag, const char* format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  log(EMERGENCY, tag, buffer);
}

void alert(const char* tag, const char* format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  log(ALERT, tag, buffer);
}

void critical(const char* tag, const char* format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  log(CRITICAL, tag, buffer);
}

void error(const char* tag, const char* format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  log(ERROR, tag, buffer);
}

void warning(const char* tag, const char* format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  log(WARNING, tag, buffer);
}

void notice(const char* tag, const char* format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  log(NOTICE, tag, buffer);
}

void info(const char* tag, const char* format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  log(INFO, tag, buffer);
}

void debug(const char* tag, const char* format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  log(DEBUG, tag, buffer);
}

} // namespace Logger
