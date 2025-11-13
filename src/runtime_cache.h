#ifndef RUNTIME_CACHE_H
#define RUNTIME_CACHE_H

#include "types.h"

namespace RuntimeCache {

// Update cached WiFi status from the main loop (Core 0)
void updateFromWiFi();

// Return a snapshot of the cached runtime status (thread-safe)
RuntimeStatus getSnapshot();

// Convenience helpers for frequently used values
bool isWifiConnected();

}

#endif // RUNTIME_CACHE_H
