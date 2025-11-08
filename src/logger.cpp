#include "logger.h"
#include "types.h"
#include <WebSerialLite.h>

extern Config Cfg;

namespace Logger {

bool webSerialEnabled = false;
Level currentLevel = LEVEL_DEBUG; // Default log level

void begin() {
  // WebSerial should be already initialized by web server
  webSerialEnabled = true;

  // Send welcome message to both Serial and WebSerial
  const char* welcomeMsg = R"(
========================================
   ESS Monitor - Logger Initialized
========================================
Version: v0.1-dev
Log Level: DEBUG
WebSerial Console: Active
----------------------------------------
All system logs will appear here.
Type 'status' for system information.
========================================
)";

  Serial.println(welcomeMsg);
  WebSerial.println(welcomeMsg);

  Serial.println("[LOGGER] Logger initialized with WebSerial support");
  Serial.printf("[LOGGER] Log level: %d (DEBUG)\n", currentLevel);
}

void setEnabled(bool en) {
  webSerialEnabled = en;
}

bool isEnabled() {
  return webSerialEnabled;
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
  // Filter by log level
  if (level > currentLevel) {
    return;
  }

  // Format message with level indicator
  const char* levelStr =
    level == LEVEL_EMERG ? "EMERG" :
    level == LEVEL_ALERT ? "ALERT" :
    level == LEVEL_CRIT ? "CRIT" :
    level == LEVEL_ERR ? "ERROR" :
    level == LEVEL_WARNING ? "WARN" :
    level == LEVEL_NOTICE ? "NOTICE" :
    level == LEVEL_INFO ? "INFO" : "DEBUG";

  char formattedMsg[512];
  snprintf(formattedMsg, sizeof(formattedMsg), "[%s][%s] %s", levelStr, tag, message);

  // Always write to Serial
  Serial.println(formattedMsg);

  // Also write to WebSerial if enabled
  if (webSerialEnabled) {
    WebSerial.println(formattedMsg);
  }
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
