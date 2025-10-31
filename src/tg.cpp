#include "tg.h"
#include "relay.h"
#include "types.h"
#include <FastBot.h>
#include <HardwareSerial.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

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

    // Check for BMS errors and warnings
    if (Ess.bmsError != previousBmsError) {
      if (Ess.bmsError > 0) {
        String errorMsg = "🚨 *КРИТИЧНА ПОМИЛКА БАТАРЕЇ!*\n\n";
        errorMsg += "⚠️ Код помилки: *" + String(Ess.bmsError) + "*\n";
        errorMsg += "Батарея може вимкнутися!\n\n";
        errorMsg += "Поточний стан:\n";
        errorMsg += "🔋 Заряд: *" + String(Ess.charge) + "%*\n";
        errorMsg += "⚡️ Напруга: *" + String(Ess.voltage, 2) + "V*\n";
        errorMsg += "🔌 Струм: *" + String(Ess.current, 1) + "A*\n";
        errorMsg += "🌡️ Температура: *" + String(Ess.temperature, 1) + "°C*\n";
        bot.sendMessage(errorMsg);
      } else if (previousBmsError > 0) {
        // Error cleared
        bot.sendMessage("✅ *Критична помилка батареї усунена.*\n\nКод помилки: " +
                        String(previousBmsError) + " → 0");
      }
      previousBmsError = Ess.bmsError;
    }

    if (Ess.bmsWarning != previousBmsWarning) {
      if (Ess.bmsWarning > 0) {
        String warningMsg = "⚠️ *ПОПЕРЕДЖЕННЯ БАТАРЕЇ*\n\n";
        warningMsg += "Код попередження: *" + String(Ess.bmsWarning) + "*\n";
        warningMsg += "Можливі причини: висока температура, напруга або розбалансування.\n\n";
        warningMsg += "Поточний стан:\n";
        warningMsg += "🔋 Заряд: *" + String(Ess.charge) + "%*\n";
        warningMsg += "⚡️ Напруга: *" + String(Ess.voltage, 2) + "V*\n";
        warningMsg += "🔌 Струм: *" + String(Ess.current, 1) + "A*\n";
        warningMsg += "🌡️ Температура: *" + String(Ess.temperature, 1) + "°C*\n";
        bot.sendMessage(warningMsg);
      } else if (previousBmsWarning > 0) {
        // Warning cleared
        bot.sendMessage("✅ *Попередження батареї усунене.*\n\nКод попередження: " +
                        String(previousBmsWarning) + " → 0");
      }
      previousBmsWarning = Ess.bmsWarning;
    }

    // Check for state changes
    if (Ess.current > (int)Cfg.tgCurrentThreshold) {
      state = State::Charging;
    } else if (Ess.current < -(int)Cfg.tgCurrentThreshold) {
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
    bot.sendMessage(getStatusMsg(), msg.chatID);
  } else if (msg.text == "/restart" || msg.text.startsWith("/restart@")) {
    if (RELAY::isEnabled()) {
      bot.sendMessage("🔄 *Запускаю процедуру перезапуску батареї...*\n\n"
                      "Реле активовано на " + String(Cfg.relayPulseMs) + "мс.\n"
                      "Імітація натискання кнопки включення BMS.", msg.chatID);
      RELAY::triggerPulse();
      bot.sendMessage("✅ *Імпульс відправлено.*\n\n"
                      "Якщо батарея не ввімкнеться, перевірте:\n"
                      "• Підключення реле до кнопки BMS\n"
                      "• Стан AUX Power Switch (має бути ON)\n"
                      "• Стан Circuit Breaker\n\n"
                      "Детальніше: BATTERY_RESTART_GUIDE.md", msg.chatID);
    } else {
      bot.sendMessage("❌ *Функція перезапуску вимкнена.*\n\n"
                      "Щоб увімкнути:\n"
                      "1. Підпайте реле до кнопки BMS\n"
                      "2. Підключіть реле до GPIO ESP32\n"
                      "3. Увімкніть функцію у веб-інтерфейсі\n\n"
                      "Детальніше: RELAY_INSTALLATION.md", msg.chatID);
    }
  }
}

String getStatusMsg() {
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
  if (Ess.charge > 75) {
    s += "🟩🟩🟩🟩";
  } else if (Ess.charge > 50) {
    s += "🟩🟩🟩🟦";
  } else if (Ess.charge > 25) {
    s += "🟩🟩🟦🟦";
  } else {
    s += "🟩🟦🟦🟦";
  }
  s += " Заряд: *" + String(Ess.charge) + "%*\n";
  s += "🔌 Навантаження: *" + String(Ess.current, 1) + "A*\n";
  s += "⚡️ Напруга: *" + String(Ess.voltage, 2) + "V*, номінальна: *" +
       String(Ess.ratedVoltage, 2) + "V*\n";
  s += "🌡️ Температура батареї: *" + String(Ess.temperature, 1) + "°C*\n";
  s += "🍀 Здоров'я батареї: *" + String(Ess.health) + "%*\n";

  // BMS errors and warnings
  if (Ess.bmsError > 0 || Ess.bmsWarning > 0) {
    s += "\n⚠️ *УВАГА!*\n";
    if (Ess.bmsError > 0) {
      s += "🚨 Критична помилка: *" + String(Ess.bmsError) + "*\n";
    }
    if (Ess.bmsWarning > 0) {
      s += "⚠️ Попередження: *" + String(Ess.bmsWarning) + "*\n";
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
