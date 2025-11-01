#ifndef _LOGGER_H_
#define _LOGGER_H_

#include <Arduino.h>

namespace Logger {

// Log levels matching syslog severity
enum Level {
  EMERGENCY = 0,  // System is unusable
  ALERT = 1,      // Action must be taken immediately
  CRITICAL = 2,   // Critical conditions
  ERROR = 3,      // Error conditions
  WARNING = 4,    // Warning conditions
  NOTICE = 5,     // Normal but significant condition
  INFO = 6,       // Informational messages
  DEBUG = 7       // Debug-level messages
};

void begin();
void setEnabled(bool enabled);
bool isEnabled();

// Logging functions
void log(Level level, const char* tag, const char* format, ...);
void emergency(const char* tag, const char* format, ...);
void alert(const char* tag, const char* format, ...);
void critical(const char* tag, const char* format, ...);
void error(const char* tag, const char* format, ...);
void warning(const char* tag, const char* format, ...);
void notice(const char* tag, const char* format, ...);
void info(const char* tag, const char* format, ...);
void debug(const char* tag, const char* format, ...);

} // namespace Logger

// Convenience macros
#define LOG_E(tag, ...) Logger::error(tag, __VA_ARGS__)
#define LOG_W(tag, ...) Logger::warning(tag, __VA_ARGS__)
#define LOG_I(tag, ...) Logger::info(tag, __VA_ARGS__)
#define LOG_D(tag, ...) Logger::debug(tag, __VA_ARGS__)

#endif
