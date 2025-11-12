#ifndef _WATCHDOG_H_
#define _WATCHDOG_H_

#include <stdint.h>

namespace WATCHDOG {

// Execution trace checkpoints for freeze diagnosis
enum TracePoint : uint8_t {
  TRACE_LOOP_START = 0,
  TRACE_HEARTBEAT_DONE = 1,
  TRACE_WIFI_CHECK_START = 2,
  TRACE_WIFI_STATUS_CALL = 3,
  TRACE_WIFI_RUN_START = 4,
  TRACE_WIFI_RUN_DONE = 5,
  TRACE_WIFI_CHECK_DONE = 6,
  TRACE_LOG_BATTERY_START = 7,
  TRACE_LOG_BATTERY_DONE = 8,
  TRACE_LOOP_END = 9,
  TRACE_UNKNOWN = 255
};

// Initialize watchdog task on Core 1 to monitor Core 0
void begin(uint8_t core, uint8_t priority);

// Update heartbeat from Core 0 (call this from main loop)
void heartbeat();

// Record execution checkpoint (for freeze diagnosis)
void trace(uint8_t checkpoint);

// Check and print crash info from RTC memory (call at startup)
void checkAndPrintCrashInfo();

} // namespace WATCHDOG

#endif
