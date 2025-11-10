#include "tg.h"
#include "can.h"
#include "types.h"
#include "logger.h"
#include <FastBot.h>
#include <HardwareSerial.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_task_wdt.h>

extern Config Cfg;
extern volatile EssStatus Ess;

namespace TG {

typedef enum State : int {
  Undef = 0,
  Charging = 1,
  Discharging = 2,
  Balance = 3
} State;

FastBot bot;
State state = State::Undef;

void begin(uint8_t core, uint8_t priority);
void task(void *pvParameters);
void loop();
void onMessage(FB_msg &msg);
String getStatusMsg();

void begin(uint8_t core, uint8_t priority) {
  xTaskCreatePinnedToCore(task, "tg_task", 20000, NULL, priority, NULL, core);
}

void task(void *pvParameters) {
  Serial.printf("[TG] Task running in core %d.\n", (uint32_t)xPortGetCoreID());

  bot.setToken(Cfg.tgBotToken);
  bot.setChatID(Cfg.tgChatID);
  bot.setTextMode(FB_MARKDOWN);
  bot.attach(onMessage);

  vTaskDelay(1000 * 30 / portTICK_PERIOD_MS);

  while (1) {
    loop();

    // Reset watchdog timer to prevent device reboot
    if (Cfg.watchdogEnabled) {
      esp_task_wdt_reset();
    }

    vTaskDelay(100 / portTICK_PERIOD_MS); // Small delay to prevent task starvation and WDT
  }

  Serial.println("[TG] Task exited.");
  vTaskDelete(NULL);
}

void loop() {
  static int16_t previousCurrent = 0;
  static State previousState = State::Undef;
  static uint8_t previousBmsError = 0;
  static uint8_t previousBmsWarning = 0;
  static uint32_t previousMillis;
  uint32_t currentMillis = millis();

  // Every 5 seconds
  if (currentMillis - previousMillis >= 1000 * 5) {
    previousMillis = currentMillis;

    // Get thread-safe copy of battery status
    EssStatus ess = CAN::getEssStatus();

    // Check for BMS errors and warnings
    if (ess.bmsError != previousBmsError) {
      if (ess.bmsError > 0) {
        String errorMsg = "🚨 *КРИТИЧНА ПОМИЛКА БАТАРЕЇ!*\n\n";
        errorMsg += "⚠️ Код помилки: *" + String(ess.bmsError) + "*\n";
        errorMsg += "Батарея може вимкнутися!\n\n";
        errorMsg += "Поточний стан:\n";
        errorMsg += "🔋 Заряд: *" + String(ess.charge) + "%*\n";
        errorMsg += "⚡️ Напруга: *" + String(ess.voltage, 2) + "V*\n";
        errorMsg += "🔌 Струм: *" + String(ess.current, 1) + "A*\n";
        errorMsg += "🌡️ Температура: *" + String(ess.temperature, 1) + "°C*\n";
        bot.sendMessage(errorMsg);
      } else if (previousBmsError > 0) {
        // Error cleared
        bot.sendMessage("✅ *Критична помилка батареї усунена.*\n\nКод помилки: " +
                        String(previousBmsError) + " → 0");
      }
      previousBmsError = ess.bmsError;
    }

    if (ess.bmsWarning != previousBmsWarning) {
      if (ess.bmsWarning > 0) {
        String warningMsg = "⚠️ *ПОПЕРЕДЖЕННЯ БАТАРЕЇ*\n\n";
        warningMsg += "Код попередження: *" + String(ess.bmsWarning) + "*\n";
        warningMsg += "Можливі причини: висока температура, напруга або розбалансування.\n\n";
        warningMsg += "Поточний стан:\n";
        warningMsg += "🔋 Заряд: *" + String(ess.charge) + "%*\n";
        warningMsg += "⚡️ Напруга: *" + String(ess.voltage, 2) + "V*\n";
        warningMsg += "🔌 Струм: *" + String(ess.current, 1) + "A*\n";
        warningMsg += "🌡️ Температура: *" + String(ess.temperature, 1) + "°C*\n";
        bot.sendMessage(warningMsg);
      } else if (previousBmsWarning > 0) {
        // Warning cleared
        bot.sendMessage("✅ *Попередження батареї усунене.*\n\nКод попередження: " +
                        String(previousBmsWarning) + " → 0");
      }
      previousBmsWarning = ess.bmsWarning;
    }

    // Check for state changes
    if (ess.current > (int)Cfg.tgCurrentThreshold) {
      state = State::Charging;
    } else if (ess.current < -(int)Cfg.tgCurrentThreshold) {
      state = State::Discharging;
    } else {
      // Let's count current deviations in a tgCurrentThreshold range as a balanced state
      state = State::Balance;
    }

    if (state != previousState && previousState != State::Undef) {
      // The state has changed from one known value to another
      switch (state) {
      case State::Discharging:
        bot.sendMessage("🕯️ *Переключено на живлення від батарейки.* Грилі не "
                        "смажимо.\n\n||" +
                        getStatusMsg() + "||");
        break;
      case State::Charging:
        bot.sendMessage("💡 *Електрохарчування відновлено.*\n\n||" +
                        getStatusMsg() + "||");
        break;
      default:
        break;
      }
    }

    previousState = state;
  }

  bot.tick();
}

void onMessage(FB_msg &msg) {
#ifdef DEBUG
  Serial.println("[TG] Message received: " + msg.toString());
#endif

  if (msg.text == "/status" || msg.text.startsWith("/status@")) {
    LOG_D("TG", "Received /status command from chat %s", msg.chatID.c_str());
    bot.sendMessage(getStatusMsg(), msg.chatID);
  } else if (msg.text == "/canstatus" || msg.text.startsWith("/canstatus@")) {
    LOG_D("TG", "Received /canstatus command from chat %s", msg.chatID.c_str());
    uint32_t keepAliveCount = CAN::getKeepAliveCounter();
    uint32_t keepAliveFailures = CAN::getKeepAliveFailures();
    uint32_t timeSinceLast = CAN::getTimeSinceLastKeepAlive();

    LOG_D("TG", "CAN stats: count=%lu, failures=%lu, lastTime=%lu ms",
          keepAliveCount, keepAliveFailures, timeSinceLast);

    String canMsg = "📡 *Статус CAN шини*\n\n";
    canMsg += "✅ Keep-alive відправлено: *" + String(keepAliveCount) + "*\n";
    canMsg += "❌ Помилок відправки: *" + String(keepAliveFailures) + "*\n";
    canMsg += "⏱️ Останній keep-alive: *" + String(timeSinceLast / 1000.0, 1) + "с* тому\n\n";

    if (timeSinceLast > 5000) {
      canMsg += "🚨 *УВАГА!* Давно не було keep-alive!\n";
      canMsg += "Батарея може відключитися через 20 хв без keep-alive.\n";
    } else if (timeSinceLast > 2000) {
      canMsg += "⚠️ Затримка з відправкою keep-alive.\n";
    } else {
      canMsg += "🟢 Keep-alive працює нормально.\n";
    }

    if (keepAliveFailures > 0) {
      canMsg += "\n⚠️ Виявлено " + String(keepAliveFailures) + " помилок відправки!\n";
      canMsg += "Можливо проблема з CAN шиною або MCP2515.\n";
    }

    bot.sendMessage(canMsg, msg.chatID);
  }
}

String getStatusMsg() {
  // Get thread-safe copy of battery status
  EssStatus ess = CAN::getEssStatus();

  String s;
  switch (state) {
  case State::Balance:
    s = "⚪️ Статус: *простій*.\n\n";
    break;
  case State::Charging:
    s = "🟢 Статус: *заряджання*.\n\n";
    break;
  case State::Discharging:
    s = "🔴 Статус: *розряджання*.\n\n";
    break;
  default:
    s = "🟡 Статус: *невизначений*.\n\n";
    break;
  }
  if (ess.charge > 75) {
    s += "🟩🟩🟩🟩";
  } else if (ess.charge > 50) {
    s += "🟩🟩🟩🟦";
  } else if (ess.charge > 25) {
    s += "🟩🟩🟦🟦";
  } else {
    s += "🟩🟦🟦🟦";
  }
  s += " Заряд: *" + String(ess.charge) + "%*\n";
  s += "🔌 Навантаження: *" + String(ess.current, 1) + "A*\n";
  s += "⚡️ Напруга: *" + String(ess.voltage, 2) + "V*, номінальна: *" +
       String(ess.ratedVoltage, 2) + "V*\n";
  s += "🌡️ Температура батареї: *" + String(ess.temperature, 1) + "°C*\n";
  s += "🍀 Здоров'я батареї: *" + String(ess.health) + "%*\n";

  // BMS errors and warnings
  if (ess.bmsError > 0 || ess.bmsWarning > 0) {
    s += "\n⚠️ *УВАГА!*\n";
    if (ess.bmsError > 0) {
      s += "🚨 Критична помилка: *" + String(ess.bmsError) + "*\n";
    }
    if (ess.bmsWarning > 0) {
      s += "⚠️ Попередження: *" + String(ess.bmsWarning) + "*\n";
    }
  } else {
    s += "\n✅ Помилок немає\n";
  }

#ifdef DEBUG
  Serial.println(s);
#endif

  return s;
}

} // namespace TG
