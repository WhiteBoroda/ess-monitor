# Исправление ошибки сборки: ModuleNotFoundError: No module named 'intelhex'

## 🔍 Проблема

```
ModuleNotFoundError: No module named 'intelhex'
```

Эта ошибка возникает когда в Python окружении PlatformIO отсутствует модуль `intelhex`, необходимый для esptool.

## ⚠️ ВАЖНО: Если у вас "No module named pip"

Если при проверке вы получили:
```
C:\Users\yv\.platformio\python3\python.exe: No module named pip
```

Это означает что **Python окружение PlatformIO повреждено**. Переходите сразу к **"Решение для поврежденного окружения"** ниже.

---

## 🆘 Решение для поврежденного окружения (если нет pip)

### Вариант A: Переустановка PlatformIO (РЕКОМЕНДУЕТСЯ)

**Шаг 1:** Закройте VS Code полностью

**Шаг 2:** Удалите папку PlatformIO (PowerShell с правами администратора):

```powershell
Remove-Item -Recurse -Force $env:USERPROFILE\.platformio
```

Или вручную удалите папку:
```
C:\Users\yv\.platformio
```

**Шаг 3:** Переустановите PlatformIO через системный Python:

```powershell
# Удалить старую версию
pip uninstall platformio

# Установить заново
pip install platformio
```

**Шаг 4:** Перезапустите VS Code

**Шаг 5:** Откройте проект и PlatformIO автоматически установит всё необходимое

**Шаг 6:** Попробуйте собрать проект:

```bash
pio run -e dev
```

---

### Вариант B: Использование системного Python

Если у вас установлен системный Python с pip:

**Шаг 1:** Найдите системный Python:

```powershell
where python
```

**Шаг 2:** Установите intelhex в системный Python:

```powershell
python -m pip install intelhex
```

**Шаг 3:** Модифицируйте `platformio.ini`:

Добавьте в секцию `[env]`:

```ini
[env]
platform = espressif32
board = esp32dev
framework = arduino
extra_scripts = pre:fix_intelhex.py
```

**Шаг 4:** Создайте файл `fix_intelhex.py` в корне проекта:

```python
Import("env")
import sys
import os

# Добавить системный Python в путь поиска модулей
sys.path.insert(0, os.path.join(os.path.expanduser("~"), "AppData", "Local", "Programs", "Python", "Python312", "Lib", "site-packages"))
```

(Измените путь на свой путь к Python)

---

### Вариант C: Ручная установка pip в PlatformIO Python

**Шаг 1:** Скачайте get-pip.py:

```powershell
Invoke-WebRequest -Uri https://bootstrap.pypa.io/get-pip.py -OutFile get-pip.py
```

**Шаг 2:** Установите pip в Python от PlatformIO:

```powershell
C:\Users\yv\.platformio\python3\python.exe get-pip.py
```

**Шаг 3:** Проверьте что pip установлен:

```powershell
C:\Users\yv\.platformio\python3\python.exe -m pip --version
```

**Шаг 4:** Установите intelhex:

```powershell
C:\Users\yv\.platformio\python3\python.exe -m pip install intelhex
```

**Шаг 5:** Соберите проект:

```bash
pio run -t clean
pio run -e dev
```

---

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
