#include "watchdog.h"
#include "types.h"
#include <Arduino.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

extern Config Cfg;

namespace WATCHDOG {

// RTC memory structure to store crash info (survives reboot)
typedef struct {
  uint32_t magic;            // Magic number to validate data
  uint32_t crashTime;        // millis() when crash detected
  uint32_t lastHeartbeat;    // Last heartbeat time
  uint32_t timeSinceBeat;    // Time since last heartbeat
  bool wasCrash;             // True if this was a watchdog crash
  uint8_t lastTracePoint;    // Last execution checkpoint before crash
  uint32_t traceTimestamp;   // Timestamp of last checkpoint
} RTC_CrashInfo;

RTC_DATA_ATTR RTC_CrashInfo rtcCrashInfo = {0};
const uint32_t CRASH_MAGIC = 0xDEADBEEF;

// Shared variable to track Core 0 heartbeat
static volatile uint32_t lastHeartbeatMillis = 0;
static portMUX_TYPE heartbeatMux = portMUX_INITIALIZER_UNLOCKED;

void begin(uint8_t core, uint8_t priority);
void task(void *pvParameters);
void heartbeat();

void begin(uint8_t core, uint8_t priority) {
  if (!Cfg.watchdogEnabled) {
    Serial.println("[WATCHDOG] Disabled by configuration");
    return;
  }

  // Initialize Hardware Watchdog Timer
  Serial.printf("[WATCHDOG] Initializing on Core %d with timeout: %d seconds\n",
                core, Cfg.watchdogTimeout);
  esp_task_wdt_init(Cfg.watchdogTimeout, true); // timeout in seconds, panic on timeout

  // Start watchdog monitoring task on specified core (should be Core 1)
  xTaskCreatePinnedToCore(task, "watchdog_task", 4096, NULL, priority, NULL, core);
}

void task(void *pvParameters) {
  Serial.printf("[WATCHDOG] Task running on Core %d\n", (uint32_t)xPortGetCoreID());

  // Add this task to WDT
  esp_task_wdt_add(NULL);
  Serial.println("[WATCHDOG] ✓ Hardware Watchdog enabled and monitoring started");

  // Wait 10 seconds before starting monitoring to allow setup() to complete
  Serial.println("[WATCHDOG] Waiting 10 seconds before starting heartbeat monitoring...");
  vTaskDelay(10000 / portTICK_PERIOD_MS);

  // Initialize heartbeat timestamp AFTER the delay
  portENTER_CRITICAL(&heartbeatMux);
  lastHeartbeatMillis = millis();
  portEXIT_CRITICAL(&heartbeatMux);

  Serial.println("[WATCHDOG] Heartbeat monitoring started!");

  uint32_t lastWarningMillis = 0;
  // HEARTBEAT_TIMEOUT: 35 seconds to account for blocking WiFi operations
  // wifiMulti.run() can block for 5-10 seconds, so we need enough margin
  const uint32_t HEARTBEAT_TIMEOUT = 35000; // 35 seconds without heartbeat = freeze!
  const uint32_t WARNING_INTERVAL = 5000;   // Print warning every 5 seconds

  while (1) {
    uint32_t currentMillis = millis();
    uint32_t lastBeat;

    // Get last heartbeat timestamp (thread-safe)
    portENTER_CRITICAL(&heartbeatMux);
    lastBeat = lastHeartbeatMillis;
    portEXIT_CRITICAL(&heartbeatMux);

    uint32_t timeSinceHeartbeat = currentMillis - lastBeat;

    // Check if Core 0 is still alive
    if (timeSinceHeartbeat > HEARTBEAT_TIMEOUT) {
      // Core 0 is frozen! Save crash info to RTC memory FIRST
      // (Serial may be blocked, so we can't rely on it)
      rtcCrashInfo.magic = CRASH_MAGIC;
      rtcCrashInfo.crashTime = currentMillis;
      rtcCrashInfo.lastHeartbeat = lastBeat;
      rtcCrashInfo.timeSinceBeat = timeSinceHeartbeat;
      rtcCrashInfo.wasCrash = true;

      // Try to log to Serial (may not work if Serial is blocked)
      // Keep it minimal and non-blocking
      Serial.printf("\n[WATCHDOG] CORE 0 FROZEN! Restarting...\n");

      // Don't wait - restart immediately
      ESP.restart();
    }

    // Print warning if heartbeat is delayed (but not frozen yet)
    // Warning at 15 seconds, freeze detection at 35 seconds
    if (timeSinceHeartbeat > 15000 && (currentMillis - lastWarningMillis > WARNING_INTERVAL)) {
      Serial.printf("[WATCHDOG] ⚠️ WARNING: Core 0 heartbeat delayed: %lu ms\n",
                    timeSinceHeartbeat);
      lastWarningMillis = currentMillis;
    }

    // Reset hardware watchdog timer (this task is alive on Core 1)
    esp_task_wdt_reset();

    // Check every 1 second
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }

  Serial.println("[WATCHDOG] Task exited (should never happen!)");
  vTaskDelete(NULL);
}

void heartbeat() {
  // Called from Core 0 main loop to indicate it's alive
  portENTER_CRITICAL(&heartbeatMux);
  lastHeartbeatMillis = millis();
  portEXIT_CRITICAL(&heartbeatMux);
}

void trace(uint8_t checkpoint) {
  // Record execution checkpoint in RTC memory (non-blocking, no serial)
  rtcCrashInfo.lastTracePoint = checkpoint;
  rtcCrashInfo.traceTimestamp = millis();
}

const char* getTracePointName(uint8_t trace) {
  switch (trace) {
    case TRACE_LOOP_START: return "Loop start";
    case TRACE_HEARTBEAT_DONE: return "After heartbeat";
    case TRACE_WIFI_CHECK_START: return "WiFi check start";
    case TRACE_WIFI_STATUS_CALL: return "WiFi.status() call";
    case TRACE_WIFI_RUN_START: return "WiFi reconnect start";
    case TRACE_WIFI_RUN_DONE: return "WiFi reconnect done";
    case TRACE_WIFI_CHECK_DONE: return "WiFi check done";
    case TRACE_LOG_BATTERY_START: return "Battery logging start";
    case TRACE_LOG_BATTERY_DONE: return "Battery logging done";
    case TRACE_LOOP_END: return "Loop end";
    default: return "Unknown";
  }
}

void checkAndPrintCrashInfo() {
  // Check if there was a watchdog crash
  if (rtcCrashInfo.magic == CRASH_MAGIC && rtcCrashInfo.wasCrash) {
    Serial.printf("\n\n");
    Serial.printf("╔═══════════════════════════════════════════════════════════╗\n");
    Serial.printf("║  ⚠️  WATCHDOG CRASH DETECTED ON PREVIOUS BOOT!           ║\n");
    Serial.printf("╚═══════════════════════════════════════════════════════════╝\n");
    Serial.printf("[WATCHDOG] Crash information from RTC memory:\n");
    Serial.printf("  • Crash detected at: %lu ms\n", rtcCrashInfo.crashTime);
    Serial.printf("  • Last heartbeat at: %lu ms\n", rtcCrashInfo.lastHeartbeat);
    Serial.printf("  • Time without heartbeat: %lu ms (%.1f seconds)\n",
                  rtcCrashInfo.timeSinceBeat,
                  rtcCrashInfo.timeSinceBeat / 1000.0);
    Serial.printf("  • Core 0 was frozen for %.1f seconds before restart!\n",
                  rtcCrashInfo.timeSinceBeat / 1000.0);
    Serial.printf("  • Last checkpoint: [%d] %s (at %lu ms)\n",
                  rtcCrashInfo.lastTracePoint,
                  getTracePointName(rtcCrashInfo.lastTracePoint),
                  rtcCrashInfo.traceTimestamp);
    Serial.printf("\n🔍 Freeze occurred after '%s' checkpoint!\n\n",
                  getTracePointName(rtcCrashInfo.lastTracePoint));

    // Clear the crash info
    rtcCrashInfo.wasCrash = false;
    rtcCrashInfo.magic = 0;
  }
}

} // namespace WATCHDOG
