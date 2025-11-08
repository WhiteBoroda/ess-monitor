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

// Mutex for thread-safe syslog access (WiFiUDP is not thread-safe)
// Using FreeRTOS mutex instead of portMUX_TYPE to avoid blocking WiFi interrupts
static SemaphoreHandle_t syslogMutex = NULL;

void begin() {
  // Create mutex for thread-safe syslog access
  if (syslogMutex == NULL) {
    syslogMutex = xSemaphoreCreateMutex();
  }

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

    // TEMPORARILY DISABLED - Debugging bootloop issue
    /*
    // Send startup message with INFO level
    syslog.logf(LEVEL_INFO, "ESS Monitor started, version: %s, log level: %d (%s)",
                VERSION, currentLevel,
                currentLevel == 0 ? "EMERG" :
                currentLevel == 1 ? "ALERT" :
                currentLevel == 2 ? "CRIT" :
                currentLevel == 3 ? "ERROR" :
                currentLevel == 4 ? "WARNING" :
                currentLevel == 5 ? "NOTICE" :
                currentLevel == 6 ? "INFO" : "DEBUG");
    */
    Serial.println("[LOGGER] NOTE: Syslog output temporarily disabled for debugging");
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

// Internal function for already-formatted messages (used by wrapper functions)
static void logFormatted(Level level, const char* tag, const char* message) {
  // Always write to Serial
  Serial.printf("[%s] %s\n", tag, message);

  // TEMPORARILY DISABLED - Debugging bootloop issue
  // TODO: Re-enable after fixing race condition
  /*
  // Send to syslog if enabled and level is <= configured level
  // Lower number = higher priority (EMERG=0, DEBUG=7)
  if (isEnabled() && WiFi.status() == WL_CONNECTED && level <= currentLevel && syslogMutex != NULL) {
    char syslogMsg[384];
    snprintf(syslogMsg, sizeof(syslogMsg), "[%s] %s", tag, message);

    // Thread-safe syslog access using FreeRTOS mutex (WiFiUDP is not thread-safe)
    // Using mutex instead of spinlock to avoid blocking WiFi interrupts
    if (xSemaphoreTake(syslogMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      syslog.log((uint16_t)level, syslogMsg);
      xSemaphoreGive(syslogMutex);
    }
  }
  */
}

void log(Level level, const char* tag, const char* format, ...) {
  char buffer[256];
  va_list args;

  // Format the message
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  // Use internal function to avoid double formatting
  logFormatted(level, tag, buffer);
}

void emergency(const char* tag, const char* format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  logFormatted(LEVEL_EMERG, tag, buffer);
}

void alert(const char* tag, const char* format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  logFormatted(LEVEL_ALERT, tag, buffer);
}

void critical(const char* tag, const char* format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  logFormatted(LEVEL_CRIT, tag, buffer);
}

void error(const char* tag, const char* format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  logFormatted(LEVEL_ERR, tag, buffer);
}

void warning(const char* tag, const char* format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  logFormatted(LEVEL_WARNING, tag, buffer);
}

void notice(const char* tag, const char* format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  logFormatted(LEVEL_NOTICE, tag, buffer);
}

void info(const char* tag, const char* format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  logFormatted(LEVEL_INFO, tag, buffer);
}

void debug(const char* tag, const char* format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  logFormatted(LEVEL_DEBUG, tag, buffer);
}

} // namespace Logger
