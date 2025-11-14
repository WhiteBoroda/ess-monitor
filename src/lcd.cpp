#include "can.h"
#include "types.h"
#include <HardwareSerial.h>
#include <U8g2lib.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdint.h>
#include <esp_task_wdt.h>

extern Config Cfg;
extern volatile EssStatus Ess;
extern RuntimeStatus Runtime;

namespace LCD {

void begin(uint8_t core, uint8_t priority);
void task(void *pvParameters);
void loop();
void draw();

U8G2_SSD1306_128X64_NONAME_F_HW_I2C *lcd = nullptr;

void begin(uint8_t core, uint8_t priority) {
  xTaskCreatePinnedToCore(task, "lcd_task", 20000, NULL, priority, NULL, core);
}

void task(void *pvParameters) {
  Serial.printf("[LCD] Task running in core %d.\n", (uint32_t)xPortGetCoreID());

  lcd = new U8G2_SSD1306_128X64_NONAME_F_HW_I2C(U8G2_R0,
                                                /* reset=*/U8X8_PIN_NONE);
  lcd->begin();
  lcd->clear();

  while (1) {
    loop();

    // Reset watchdog timer to prevent device reboot
    if (Cfg.watchdogEnabled) {
      esp_task_wdt_reset();
    }

    // Safety delay in case loop() returns early
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }

  Serial.println("[LCD] Task exited.");
  vTaskDelete(NULL);
}

void loop() {
  vTaskDelay(3000 / portTICK_PERIOD_MS);
  draw();
#ifdef DEBUG
  Serial.println("[LCD] Draw.");
#endif
}

void draw() {
  // Get thread-safe copy of battery status
  EssStatus ess = CAN::getEssStatus();
  RuntimeStatus runtime = Runtime;

  lcd->clearBuffer();
  lcd->setFont(u8g2_font_pressstart2p_8r);

  // Voltage
  lcd->drawStr(0, 10, "V");

  lcd->setCursor(12, 10);
  lcd->print(ess.voltage);

  lcd->setCursor(70, 10);
  lcd->print(ess.ratedVoltage);

  lcd->drawLine(0, 11, 128, 11);

  // Charge / health
  lcd->drawStr(0, 24, "C:");

  lcd->setCursor(16, 24);
  lcd->print(ess.charge);
  lcd->drawStr(36, 24, "%");

  lcd->drawStr(70, 24, "H:");
  lcd->setCursor(86, 24);
  lcd->print(ess.health);
  lcd->drawStr(110, 24, "%");

  // Temperature
  lcd->drawStr(12, 36, "Temp:");
  lcd->setCursor(52, 36);
  lcd->print(ess.temperature);
  lcd->drawStr(102, 36, "C");

  // BMS errors or Wifi status
  if (ess.bmsError || ess.bmsWarning) {
    lcd->drawStr(0, 50, "ER/WR:");
    lcd->setCursor(50, 50);
    lcd->print(ess.bmsError);
    lcd->setCursor(90, 50);
    lcd->print(ess.bmsWarning);
  } else {
    // CRITICAL FIX: Use cached WiFi status from Core 0 (main loop)
    // WiFi.status() is not thread-safe when called from Core 1 (LCD task)
    if (runtime.wifiConnected) {
      lcd->drawStr(0, 50, "IP:");
      lcd->drawStr(18, 50, runtime.cachedIP);
    } else {
      lcd->drawStr(0, 50, "AP:");
      lcd->drawStr(18, 50, Cfg.hostname);
    }
  }

  // Current & limits
  lcd->drawStr(0, 64, "A");
  lcd->setCursor(12, 64);
  lcd->print(ess.current);
  lcd->setCursor(70, 64);
  lcd->print(ess.ratedChargeCurrent, 0);
  lcd->setCursor(100, 64);
  lcd->print(ess.ratedDischargeCurrent, 0);
  lcd->drawLine(0, 52, 128, 52);

  lcd->sendBuffer();
}

} // namespace LCD
