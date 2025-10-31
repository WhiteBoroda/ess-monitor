# Исправление ошибки сборки: ModuleNotFoundError: No module named 'intelhex'

## 🔍 Проблема

```
ModuleNotFoundError: No module named 'intelhex'
```

Эта ошибка возникает когда в Python окружении PlatformIO отсутствует модуль `intelhex`, необходимый для esptool.

---

## ✅ Решение 1: Установка intelhex (Рекомендуется)

### Windows (PowerShell или CMD):

```powershell
# Найти Python от PlatformIO
C:\Users\yv\.platformio\python3\python.exe -m pip install intelhex
```

Или через полный путь к pip:

```powershell
C:\Users\yv\.platformio\python3\Scripts\pip.exe install intelhex
```

### Альтернативный способ (если первый не работает):

```powershell
# Использовать системный Python
python -m pip install intelhex
```

---

## ✅ Решение 2: Переустановка tool-esptoolpy

### Через PlatformIO CLI:

```bash
# Удалить текущий esptoolpy
pio pkg uninstall -g --tool "tool-esptoolpy"

# Установить заново
pio pkg install -g --tool "tool-esptoolpy"
```

### Или через Visual Studio Code:

1. Откройте **PlatformIO Home**
2. Перейдите в **Platforms**
3. Найдите **Espressif 32**
4. Нажмите **Uninstall** (если установлена)
5. Нажмите **Install** заново

---

## ✅ Решение 3: Обновление PlatformIO Core

```bash
# Обновить PlatformIO Core
pio upgrade

# Или через pip
pip install -U platformio
```

После обновления перезапустите VS Code.

---

## ✅ Решение 4: Очистка кеша и пересборка

```bash
# Очистить кеш PlatformIO
pio pkg update

# Удалить папку .pio
# Windows PowerShell:
Remove-Item -Recurse -Force .pio

# Пересобрать проект
pio run
```

---

## ✅ Решение 5: Установка через requirements.txt (универсальное)

Создайте файл `requirements.txt` в корне проекта:

```txt
intelhex>=2.3.0
```

Затем установите:

```bash
C:\Users\yv\.platformio\python3\python.exe -m pip install -r requirements.txt
```

---

## 🔧 Проверка после установки

```bash
# Проверить что intelhex установлен
C:\Users\yv\.platformio\python3\python.exe -m pip list | findstr intelhex

# Должно вывести:
# intelhex    2.3.0
```

---

## 🚀 Пересборка проекта

После установки intelhex:

```bash
# Очистить предыдущую сборку
pio run -t clean

# Собрать заново
pio run -e dev
```

Или через VS Code:
- **PlatformIO: Clean**
- **PlatformIO: Build**

---

## ❗ Если ничего не помогло

### Вариант A: Использовать другую версию платформы

Откройте `platformio.ini` и измените версию платформы:

```ini
[env]
platform = espressif32@6.4.0  # Вместо самой последней
board = esp32dev
framework = arduino
```

### Вариант B: Использовать Docker

Если у вас установлен Docker:

```bash
docker pull infinitecoding/platformio-for-ci
docker run --rm -v ${PWD}:/workspace -w /workspace infinitecoding/platformio-for-ci pio run
```

### Вариант C: Переустановка PlatformIO полностью

```bash
# Удалить папку PlatformIO
Remove-Item -Recurse -Force C:\Users\yv\.platformio

# Переустановить PlatformIO
pip uninstall platformio
pip install platformio

# Перезапустить VS Code
```

---

## 📋 Диагностика

### Проверить версии:

```bash
# Версия PlatformIO
pio --version

# Версия Python от PlatformIO
C:\Users\yv\.platformio\python3\python.exe --version

# Список установленных пакетов
C:\Users\yv\.platformio\python3\python.exe -m pip list
```

### Проверить пути:

```bash
# Где находится esptool
where esptool

# Где находится Python от PlatformIO
where python
```

---

## 🎯 Быстрое решение (One-liner)

**Для большинства случаев это должно помочь:**

```powershell
C:\Users\yv\.platformio\python3\python.exe -m pip install intelhex && pio run -t clean && pio run -e dev
```

---

## 💡 Почему это происходит?

1. **Обновление esptool** - новая версия требует intelhex, а PlatformIO не обновил зависимости
2. **Поврежденная установка** - Python окружение PlatformIO повреждено
3. **Конфликт версий** - разные версии Python на системе

---

## ✅ Рекомендуемая последовательность действий

**Шаг 1:** Попробуйте быстрое решение (One-liner выше)

**Шаг 2:** Если не помогло, попробуйте Решение 1 (установка intelhex)

**Шаг 3:** Если не помогло, попробуйте Решение 2 (переустановка tool-esptoolpy)

**Шаг 4:** Если не помогло, попробуйте Решение 3 (обновление PlatformIO)

**Шаг 5:** Если не помогло, напишите мне результаты диагностики

---

**Создано:** 2025-10-31
**Версия:** 1.0
