#ifndef _WATCHDOG_H_
#define _WATCHDOG_H_

#include <stdint.h>

namespace WATCHDOG {

// Initialize watchdog task on Core 1 to monitor Core 0
void begin(uint8_t core, uint8_t priority);

// Update heartbeat from Core 0 (call this from main loop)
void heartbeat();

} // namespace WATCHDOG

#endif
