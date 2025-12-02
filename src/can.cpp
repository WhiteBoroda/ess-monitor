#include "can.h"
#include "logger.h"
#include "types.h"
#include <HardwareSerial.h>
#include <SPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mcp_can.h>
#include <stdint.h>
#include <esp_task_wdt.h>

extern volatile EssStatus Ess;
extern Config Cfg;

namespace CAN {

MCP_CAN can(CS_PIN);
portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE keepAliveMux = portMUX_INITIALIZER_UNLOCKED;

// Keep-alive monitoring
static uint32_t keepAliveCounter = 0;
static uint32_t keepAliveFailures = 0;
static uint32_t lastKeepAliveMillis = 0;

// CAN initialization status
static bool canInitialized = false;

void begin(uint8_t core, uint8_t priority);
void task(void *pvParameters);
void loop();
bool initCAN();
void readCAN();
void writeCAN();
void logReadDataFrame(DataFrame *f);
void logWriteDataFrame(DataFrame *f);
int16_t bytesToInt16(uint8_t low, uint8_t high);
void processDataFrame(DataFrame *f);
uint8_t getChargeControlByte();
DataFrame getChargeDataFrame();

void begin(uint8_t core, uint8_t priority) {
  xTaskCreatePinnedToCore(task, "can_task", 20000, NULL, priority, NULL, core);
}

void task(void *pvParameters) {
  Serial.printf("[CAN] Task running in core %d.\n", (uint32_t)xPortGetCoreID());

  if (initCAN()) {
    while (1) {
      loop();

      // Reset watchdog timer to prevent device reboot
      if (Cfg.watchdogEnabled) {
        esp_task_wdt_reset();
      }

      vTaskDelay(10 / portTICK_PERIOD_MS); // Small delay to prevent WDT
    }
  }

  Serial.println("[CAN] Task exited.");
  vTaskDelete(NULL);
};

bool initCAN() {
  LOG_I("CAN", "Initializing MCP2515 CAN controller...");

  if (can.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
    can.setMode(MCP_NORMAL);
    // Use polling instead of interrupt to prevent CPU lockup
    // readCAN() is called from loop() every 10ms which is sufficient
    pinMode(INT_PIN, INPUT);
    canInitialized = true;
    LOG_I("CAN", "✓ MCP2515 initialized successfully at 500KBPS");
    LOG_I("CAN", "CAN bus is active and ready");
    return true;
  }

  canInitialized = false;
  LOG_E("CAN", "✗ Failed to initialize MCP2515 CAN controller");
  LOG_W("CAN", "MCP2515 module not detected or not connected");
  LOG_W("CAN", "Please check:");
  LOG_W("CAN", "  - MCP2515 module is connected to SPI pins");
  LOG_W("CAN", "  - CS pin is correct (GPIO %d)", CS_PIN);
  LOG_W("CAN", "  - INT pin is correct (GPIO %d)", INT_PIN);
  LOG_W("CAN", "Device will continue without CAN functionality");
  LOG_W("CAN", "Battery data will not be available");
  return false;
}

void loop() {
  static uint32_t previousMillis;
  uint32_t currentMillis = millis();

  readCAN();

  // Send keep-alive at configured interval (default: 3 seconds)
  if (currentMillis - previousMillis >= Cfg.canKeepAliveInterval) {
    previousMillis = currentMillis;
    writeCAN();
  }
}

void readCAN() {
  if (digitalRead(INT_PIN)) {
    // INT_PIN high state means there is nothing to read
    return;
  }

  DataFrame f = {};
  can.readMsgBuf((unsigned long *)&f.id, &f.dlc, f.data);
  // logReadDataFrame(&f);
  processDataFrame(&f);
}

void writeCAN() {
  DataFrame chargeFrame = getChargeDataFrame();
  byte sendStatus;
  static uint32_t lastErrorLogTime = 0;

  // Send 0x35E - Protocol ID
  sendStatus = can.sendMsgBuf(DF_35E.id, DF_35E.dlc, (uint8_t *)DF_35E.data);

  // Send 0x305 - KEEP-ALIVE (CRITICAL!)
  // Note: Removed logWriteDataFrame() here to avoid WDT timeout from Serial.printf()
  sendStatus = can.sendMsgBuf(DF_305.id, DF_305.dlc, (uint8_t *)DF_305.data);

  // Update counters (quick, inside critical section)
  bool keepAliveOk = (sendStatus == CAN_OK);
  uint32_t failures = 0;
  uint32_t counter = 0;

  portENTER_CRITICAL(&keepAliveMux);
  if (keepAliveOk) {
    keepAliveCounter++;
    lastKeepAliveMillis = millis();
    counter = keepAliveCounter;
  } else {
    keepAliveFailures++;
    failures = keepAliveFailures;
  }
  portEXIT_CRITICAL(&keepAliveMux);

  // Log OUTSIDE critical section, and only every 10 seconds to avoid WDT timeout
  uint32_t now = millis();
  if (now - lastErrorLogTime >= 10000) {
    if (!keepAliveOk) {
      LOG_E("CAN", "Keep-alive send failures: %lu (logged every 10s)", failures);
    }

    // Check for missed keep-alives
    portENTER_CRITICAL(&keepAliveMux);
    uint32_t lastMillis = lastKeepAliveMillis;
    portEXIT_CRITICAL(&keepAliveMux);

    uint32_t timeSinceLastKeepAlive = now - lastMillis;
    if (timeSinceLastKeepAlive >= Cfg.canKeepAliveInterval && lastMillis > 0) {
      LOG_W("CAN", "WARNING: %lu ms since last successful keep-alive!", timeSinceLastKeepAlive);
    }

    lastErrorLogTime = now;
  }

  // Send 0x35C - Charge control
  can.sendMsgBuf(chargeFrame.id, chargeFrame.dlc, chargeFrame.data);
}

void logReadDataFrame(DataFrame *f) {
#ifdef DEBUG
  Serial.printf("[CAN] Frame received:\t <- ID: %04x DLC: %d Data: "
                "%02x %02x %02x %02x %02x %02x %02x %02x\n",
                f->id, f->dlc, f->data[0], f->data[1], f->data[2], f->data[3],
                f->data[4], f->data[5], f->data[6], f->data[7]);
#endif
}

void logWriteDataFrame(DataFrame *f) {
#ifdef DEBUG
  Serial.printf("[CAN] Frame sent:\t -> ID: %04x DLC: %d Data: "
                "%02x %02x %02x %02x %02x %02x %02x %02x\n",
                f->id, f->dlc, f->data[0], f->data[1], f->data[2], f->data[3],
                f->data[4], f->data[5], f->data[6], f->data[7]);
#endif
}

int16_t bytesToInt16(uint8_t low, uint8_t high) { return (high << 8) | low; }

void processDataFrame(DataFrame *f) {
  portENTER_CRITICAL(&stateMux);
  switch (f->id) {
  case 849: // 0x351 Battery Limits
    Ess.ratedVoltage = bytesToInt16(f->data[0], f->data[1]) / 10.0;
    Ess.ratedChargeCurrent = bytesToInt16(f->data[2], f->data[3]) / 10.0;
    Ess.ratedDischargeCurrent = bytesToInt16(f->data[4], f->data[5]) / 10.0;
    break;
  case 853: // 0x355 Battery Health
    Ess.charge = bytesToInt16(f->data[0], f->data[1]);
    Ess.health = bytesToInt16(f->data[2], f->data[3]);
    break;
  case 854: // 0x356 System Voltage, Current, Temp
    Ess.voltage = bytesToInt16(f->data[0], f->data[1]) / 100.0;
    Ess.current = bytesToInt16(f->data[2], f->data[3]) / 10.0;
    Ess.temperature = bytesToInt16(f->data[4], f->data[5]) / 10.0;
    break;
  case 857: // 0x359 BMS Error
    Ess.bmsProtection = f->data[0];
    Ess.bmsWarning = f->data[1];
    Ess.bmsError = f->data[3];
    break;
  }
  portEXIT_CRITICAL(&stateMux);
}

uint8_t getChargeControlByte() {
  uint8_t res = 0;
  if (false) { // TODO: if(data.full_charge_time)
    res |= (1 << 3);
  }
  if (Ess.ratedDischargeCurrent != 0 && Ess.charge > 30) {
    // Battery allows discharging and discharge policy is ok.
    // TODO: read target discharge value from settings
    res |= (1 << 6);
  }
  if (Ess.ratedChargeCurrent != 0 && Ess.charge < 98) {
    // Battery allows charging and charge policy is ok.
    // TODO: read target charge value from settings
    res |= (1 << 7);
  }
  return res;
}

DataFrame getChargeDataFrame() {
  DataFrame f = {0x35c, 2, {getChargeControlByte(), 0x00}};
  return f;
}

uint32_t getKeepAliveCounter() {
  portENTER_CRITICAL(&keepAliveMux);
  uint32_t count = keepAliveCounter;
  portEXIT_CRITICAL(&keepAliveMux);
  return count;
}

uint32_t getKeepAliveFailures() {
  portENTER_CRITICAL(&keepAliveMux);
  uint32_t failures = keepAliveFailures;
  portEXIT_CRITICAL(&keepAliveMux);
  return failures;
}

uint32_t getTimeSinceLastKeepAlive() {
  portENTER_CRITICAL(&keepAliveMux);
  uint32_t lastMillis = lastKeepAliveMillis;
  portEXIT_CRITICAL(&keepAliveMux);

  if (lastMillis == 0) {
    return 0;
  }
  return millis() - lastMillis;
}

EssStatus getEssStatus() {
  EssStatus copy;
  portENTER_CRITICAL(&stateMux);
  copy.charge = Ess.charge;
  copy.health = Ess.health;
  copy.voltage = Ess.voltage;
  copy.current = Ess.current;
  copy.ratedVoltage = Ess.ratedVoltage;
  copy.ratedChargeCurrent = Ess.ratedChargeCurrent;
  copy.ratedDischargeCurrent = Ess.ratedDischargeCurrent;
  copy.temperature = Ess.temperature;
  copy.bmsProtection = Ess.bmsProtection;
  copy.bmsWarning = Ess.bmsWarning;
  copy.bmsError = Ess.bmsError;
  portEXIT_CRITICAL(&stateMux);
  return copy;
}

String getBmsStatusString() {
  EssStatus s = getEssStatus();
  String status = "";

  if (s.bmsWarning == 0 && s.bmsError == 0 && s.bmsProtection == 0) {
    return "Normal";
  }
  
  // 1. Check specific codes first
  if (s.bmsError == 31) return "Error 31: Critical System Failure";
  if (s.bmsError == 209) return "Error 209: Critical Logic/Comm Failure";
  if (s.bmsError == 4) return "Error 4: Sensor/Circuit Failure";
  if (s.bmsWarning == 192) {
    // 0xC0 = High Current Discharge + High Current Charge
    // BUT in some LG/Pylontech implementations this combo means Imbalance/System Warning
    // If temp is normal (<45C) and voltage is normal (<58V), it is likely High Current or Imbalance.
    return "Warning 192: High Current / Imbalance";
  }
  if (s.bmsWarning == 2) return "Warning 2: Internal/Temp Warning";
  if (s.bmsWarning == 8) return "Warning 8: High Voltage/Balancing";

  // 2. Protection Flags (Byte 0)
  if (s.bmsProtection > 0) {
    status += "[PROT] ";
    if (s.bmsProtection & 0x80) status += "Dischg OverCur, ";
    if (s.bmsProtection & 0x40) status += "Chg OverCur, ";
    if (s.bmsProtection & 0x20) status += "Short Circ, ";
    if (s.bmsProtection & 0x10) status += "Load Short, ";
    if (s.bmsProtection & 0x08) status += "Cell UnderVolt, ";
    if (s.bmsProtection & 0x04) status += "Cell OverVolt, ";
    if (s.bmsProtection & 0x02) status += "Cell UnderTemp, ";
    if (s.bmsProtection & 0x01) status += "Cell OverTemp, ";
  }

  // 3. Warning Flags (Byte 1)
  if (s.bmsWarning > 0) {
    status += "[WARN] ";
    if (s.bmsWarning & 0x80) status += "Dischg HighCur, ";
    if (s.bmsWarning & 0x40) status += "Chg HighCur, ";
    if (s.bmsWarning & 0x20) status += "High Temp, ";
    if (s.bmsWarning & 0x10) status += "Low Temp, ";
    if (s.bmsWarning & 0x08) status += "High Volt, ";
    if (s.bmsWarning & 0x04) status += "Low Volt, ";
    if (s.bmsWarning & 0x02) status += "Internal Err, ";
    if (s.bmsWarning & 0x01) status += "System Err, ";
    
    status += "(Code " + String(s.bmsWarning) + ") ";
  }

  // 4. Error Flags (Byte 3)
  if (s.bmsError > 0) {
    status += "[ERR] ";
    if (s.bmsError & 0x80) status += "Inv Comm, ";
    if (s.bmsError & 0x40) status += "Int Logic, ";
    if (s.bmsError & 0x20) status += "Comm Err, ";
    if (s.bmsError & 0x10) status += "Cell T-Sens, ";
    if (s.bmsError & 0x08) status += "Cell V-Sens, ";
    if (s.bmsError & 0x04) status += "Temp Sens, ";
    if (s.bmsError & 0x02) status += "Dsg MOS, ";
    if (s.bmsError & 0x01) status += "Chg MOS, ";

    status += "(Code " + String(s.bmsError) + ")";
  }
  
  // Clean up trailing comma
  if (status.endsWith(", ")) {
    status = status.substring(0, status.length() - 2);
  }

  return status;
}

bool isInitialized() {
  return canInitialized;
}

} // namespace CAN
