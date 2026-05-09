```markdown
# Chess Game

Сетевая шахматная игра с рейтинговой системой, очередью игроков и поддержкой ничьи.

## Содержание

- [Требования](#требования)
- [Установка и сборка](#установка-и-сборка)
  - [Windows](#windows)
  - [Linux](#linux)
- [Запуск](#запуск)
- [Команды клиента](#команды-клиента)
- [Сетевой протокол](#сетевой-протокол)
- [Структура проекта](#структура-проекта)
- [Тестирование](#тестирование)

## Требования

- C++17 компилятор (g++ 8+, clang 7+, MSVC 2019+)
- Boost 1.70+ (библиотеки: system, asio, log, thread, chrono, atomic)
- CMake 3.10+ (рекомендуется) или Make
- Python 3.8+ (для тестов)

## Установка и сборка

### Windows

1. Установите Boost в удобную директорию, например `D:\boost_1_90_0`

2. Отредактируйте `build.bat` под ваши пути к Boost:

```batch
-I"D:\путь\к\boost_1_90_0"
-L"D:\путь\к\boost_1_90_0\stage\lib"
```

3. Запустите сборку:

```bash
build.bat
```

После сборки появятся:
- `server.exe` - сервер
- `client.exe` - клиент

### Linux

#### Установка зависимостей

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install g++ cmake libboost-system-dev libboost-asio-dev libboost-log-dev libboost-thread-dev
```

**Fedora:**
```bash
sudo dnf install gcc-c++ cmake boost-devel
```

**Arch Linux:**
```bash
sudo pacman -S gcc cmake boost
```

#### Сборка через CMake

```bash
# Клонирование репозитория
git clone https://github.com/MacMellon6651/Chess-Game.git
cd Chess-Game

# Создание директории для сборки
mkdir build && cd build

# Генерация файлов сборки
cmake ..

# Компиляция
make -j$(nproc)
```

#### Сборка через Makefile (если нет CMake)

```bash
# Компиляция сервера
g++ -std=c++17 main.cpp server.cpp command.cpp config.cpp logging.cpp -o server \
    -lboost_system -lboost_log -lboost_thread -lboost_chrono -lboost_atomic -lpthread

# Компиляция клиента
g++ -std=c++17 client.cpp command.cpp config.cpp logging.cpp -o client \
    -lboost_system -lboost_log -lboost_thread -lboost_chrono -lboost_atomic -lpthread
```

#### Настройка

Создайте `settings.json`:

```json
{
    "host": "127.0.0.1",
    "port": 18080
}
```

## Запуск

### Запуск сервера

**Windows:**
```bash
server.exe
```

**Linux:**
```bash
./server
```

### Запуск клиентов

Откройте два окна терминала, в каждом выполните:

**Windows:**
```bash
client.exe
```

**Linux:**
```bash
./client
```

При первом запуске клиент запросит ник.

## WSS (SSL) с самоподписанным сертификатом

Для полноценного защищенного websocket-канала (`wss://`) используется TLS-прокси с собственным сертификатом:

1. Сгенерируйте сертификат и установите его в доверенные корни текущего пользователя:
```bash
npm run generate:wss-cert
```
2. Запустите сервер игры (обычный `ws://127.0.0.1:18081`).
3. Запустите TLS-прокси:
```bash
npm run start:wss-proxy
```
4. Запустите web-client. По умолчанию он подключится к `wss://localhost:18082`.

Переменные окружения:
- `REACT_APP_WS_URL` - явный websocket URL (например `wss://localhost:18082`)
- `REACT_APP_FORCE_INSECURE_WS=true` - принудительно использовать `ws://localhost:18081`

## Команды клиента

| Команда | Описание |
|---------|----------|
| `/queue` | Встать в очередь поиска соперника |
| `/leave` | Выйти из очереди или текущей игры |
| `/move e2 e4` | Сделать ход (шахматная нотация) |
| `/chat текст` | Отправить сообщение сопернику |
| `/draw` | Предложить ничью |
| `/draw accept` | Принять ничью |
| `/draw decline` | Отклонить ничью |
| `/quit` | Выйти из клиента |

## Сетевой протокол

### Клиент -> Сервер

```json
{"type": "auth", "nick": "Alice"}
{"type": "queue"}
{"type": "move", "from": "e2", "to": "e4"}
{"type": "chat", "text": "Hello"}
{"type": "draw", "action": "offer"}
{"type": "ping"}
```

### Сервер -> Клиент

```json
{"status": "ok", "message": "authorized"}
{"status": "error", "message": "Invalid format"}
{"type": "event", "event": "match_found", "data": {"opponent": "Bob"}}
{"type": "event", "event": "move", "data": {"from": "e2", "to": "e4", "by": "Alice"}}
{"type": "event", "event": "chat", "data": {"from": "Alice", "text": "Hi"}}
{"type": "event", "event": "draw_offer", "data": {"from": "Bob"}}
{"type": "event", "event": "draw_agreed"}
{"type": "event", "event": "opponent_disconnected"}
{"type": "event", "event": "rating_update", "data": {"rating": 1032}}
```

## Структура проекта

| Файл | Назначение |
|------|------------|
| `main.cpp` | Точка входа сервера |
| `server.cpp/hpp` | Логика сервера |
| `client.cpp` | Консольный клиент |
| `command.cpp/hpp` | Парсинг JSON-команд |
| `config.cpp/hpp` | Загрузка настроек |
| `logging.cpp/hpp` | Логирование |
| `json.hpp` | Библиотека nlohmann/json |
| `chess_engine/` | Шахматный движок |
| `test_*.py` | Тесты |

## Тестирование

```bash
pip install pytest
pytest test_server_stress.py -v
pytest test_json_logic.py -v
```
