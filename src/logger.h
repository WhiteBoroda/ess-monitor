#ifndef _LOGGER_H_
#define _LOGGER_H_

#include <Arduino.h>
#include <stdarg.h>

namespace Logger {

// Log levels matching syslog severity
// Using LEVEL_ prefix to avoid conflicts with Syslog library macros
typedef enum {
  LEVEL_EMERG = 0,   // System is unusable
  LEVEL_ALERT = 1,   // Action must be taken immediately
  LEVEL_CRIT = 2,    // Critical conditions
  LEVEL_ERR = 3,     // Error conditions
  LEVEL_WARNING = 4, // Warning conditions
  LEVEL_NOTICE = 5,  // Normal but significant condition
  LEVEL_INFO = 6,    // Informational messages
  LEVEL_DEBUG = 7    // Debug-level messages
} Level;

void begin();
void setEnabled(bool enabled);
bool isEnabled();
void setLevel(Level level);
Level getLevel();

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
