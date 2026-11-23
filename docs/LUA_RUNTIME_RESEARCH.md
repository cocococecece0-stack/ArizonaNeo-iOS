# Lua Runtime Research (Stage 6.0)

Дата: 2026-11-23.
Репозиторий: `/root/ArizonaNeo-iOS`.
Исследовательские материалы: `/root/ArizonaNeo-research` (NeoLoader 1.0.72 build 172).
LuaJIT исходники: `/tmp/opencode/luajit` (не добавлены в Git).

## 1. Объём анализа

- Изучено `.lua` файлов: **210** (включая корневые скрипты, `lib/`, подкаталоги).
- Метод: статическое чтение (`cat`/`sed`/`grep`), без выполнения скриптов.
- Различались вызовы, объявления и комментарии (комментарии вида `-- This file is part of SA MoonLoader package` не учитывались как использование `package`).

## 2. Инвентарь использования Lua API

Счётчики — количество вхождений (occ) и файлов (files), найденных grep по шаблону. Для `ffi.*` подтверждено чтением исходников.

| API / механизм | occ | files | Роль | Поддержка на iOS | Замены |
|---|---:|---:|---|---|---|
| `require(...)` | 286 | 64 | Загрузка модулей | Да (собственный loader) | Реализовать module loader с sandbox paths |
| `package.*` | 6 | 5 | `package.loaded` (bass.lua, copas.lua) | Да | Реализовать `package.loaded` |
| `ffi` (любое) | 1244 | 118 | FFI-доступ | Частично (см. FFI) | Адаптация |
| `ffi.cdef` | 108 | 104 | Объявления C-типов/функций | Да (interpreter FFI) | Проверка cdef |
| `ffi.load` | 13 | 10 | Загрузка нативных библиотек | Ограниченно | Статическая линковка/bridge |
| `ffi.C.*` | 511 | 6 | Вызов C-функций (ImGui, JNI, mprotect, BASS) | Зависит от символов | iOS bridge |
| `ffi.cast` | 92 | 16 | Приведение типов/указателей | Да | — |
| `ffi.new` | 75 | 12 | Создание cdata | Да | — |
| `ffi.string` | 61 | 13 | Чтение C-строк | Да | — |
| `ffi.copy` | 16 | 4 | Копирование памяти | Да | — |
| `ffi.fill` | 9 | 2 | Заполнение памяти | Да | — |
| `ffi.metatype` | 60 | 6 | Метатипы (ImVec2 и др.) | Да | — |
| `ffi.gc` | 66 | 2 | GC-хуки для cdata | Да | — |
| `ffi.typeof/sizeof/istype` | 85 | 21 | Интроспекция типов | Да | — |
| `bit` | 208 | 22 | Битовые операции | Да (LuaJIT bit) | Lua 5.1: bit32 или реализация |
| `jit` | 67 | 13 | `jit.os`, `jit.arch`, `jit.status` | Частично (JIT выключен) | Проверка `jit.status` |
| `coroutine.*` | 31 | 8 | Корутины | Да | — |
| `debug.*` | 12 | 4 | Отладка | Да | — |
| `loadstring` | 6 | 4 | Загрузка кода из строки | Да | — |
| `setfenv` | 2 | 1 | Окружения (Lua 5.1) | Да (LuaJIT 5.1 compat) | — |
| `getfenv` | 0 | 0 | — | — | — |
| `io.*` | 181 | 26 | Файловый I/O | Частично (sandbox) | SandboxedFilesystem |
| `os.*` | 597 | 21 | Время, пути, окружение | Частично | iOS-адаптеры |
| `package.loadlib` | 0 | 0 | — | — | — |
| `package.path/cpath` | 0 | 0 | — | — | собственный loader |
| `memory.*` | 56 | 3 | Чтение/запись памяти | Ограниченно | GameBridge |
| `mprotect` | 3 | 1 | Защита страниц | Да (POSIX) | — |
| `callback` (слово) | 159 | 16 | Коллбэки (в основном комментарии/имена) | Проверить | — |
| `ffi.callback` | 0 | 0 | Нативные коллбэки через FFI | Да, но с ограничениями | — |
| `cdata` | 55 | 3 | Упоминания | — | — |
| `uintptr_t`/`void*` | 82 | 18 | Указатели | Да (cdata) | — |
| `memory access` (offsets) | — | — | GTA SA offsets | Не подтверждено на iOS | GameBridge |

Ключевые наблюдения:
- Скрипты интенсивно используют FFI (`ffi.cdef`, `ffi.cast`, `ffi.new`, `ffi.string`, `ffi.metatype`, `ffi.gc`) — FFI обязателен.
- `ffi.load` используется для `"GTASA"`, `"monetloader"`, MoonMonet `.so` и `NeoMLoader` (по строкам). На iOS эти библиотеки недоступны; требуется bridge/статическая линковка.
- `ffi.callback` напрямую не используется.
- `package.loadlib` не используется — загрузка `.so` через `ffi.load`/`package.loaded`.
- `bit` используется (22 файла).
- `jit.os`/`jit.arch` используются для условной логики.
- `setfenv` используется (1 файл) — нужна Lua 5.1 совместимость.
- Корутины (`coroutine.*`) используются (8 файлов).
- `io`/`os` используются широко — нужны sandbox-адаптеры.

## 3. LuaJIT 2.1 исследование

Источник: официальный репозиторий `https://github.com/LuaJIT/LuaJIT` (ветка v2.1, коммит 96f7b29).

### 3.1. ARM64 support
- LuaJIT поддерживает ARM64 (AARCH64) — `src/lj_arch.h` содержит `LJ_TARGET_ARM64`.
- Сборка для arm64 подтверждена Makefile (`ifeq (arm64,$(TARGET_LJARCH))`).

### 3.2. iOS target
- `LJ_TARGET_IOS` определяется при `TARGET_OS_IPHONE` или `LUAJIT_TARGET_IPHONE`.
- На iOS JIT отключается по умолчанию: `LJ_OS_NOJIT = 1` (если не определён `LUAJIT_ENABLE_JIT`).
- FFI не отключается на iOS: `LJ_ARCH_NOFFI` определён только для PPC/console; для ARM64 iOS FFI остаётся включённым.

### 3.3. Interpreter-only build
- Макросы:
  - `LJ_HASJIT = 0` если `LUAJIT_DISABLE_JIT` или `LJ_ARCH_NOJIT` или `LJ_OS_NOJIT`.
  - `LJ_HASFFI = 0` если `LUAJIT_DISABLE_FFI` или `LJ_ARCH_NOFFI`.
- На iOS: JIT выключен, FFI включён (по умолчанию). **FFI сохраняется при выключенном JIT** — подтверждено макросами и тестом.

### 3.4. FFI с выключенным JIT
- Проверено на Linux-сборке с `XCFLAGS="-DLUAJIT_DISABLE_JIT"`:
  - `jit.status()` → "no jit".
  - `require("ffi")` работает.
  - FFI-тест (структура, поля, вызов `puts`) проходит.
- Вывод: FFI доступен в interpreter-only режиме.

### 3.5. bit / jit namespace / Lua 5.1
- `lib_bit.c` присутствует — `bit` доступен.
- `jit` namespace присутствует (`lib_jit.c`) — `jit.status`, `jit.os`, `jit.arch`.
- LuaJIT совместим с Lua 5.1 API (включая `setfenv`/`getfenv`).
- C API для встраивания: `lua.h`, `lauxlib.h`, `lualib.h` (совместимы с 5.1).

### 3.6. Статическая библиотека
- `make` собирает `libluajit.a` (static) и `libluajit.so` (dynamic).
- Для iOS используется `BUILDMODE=static` (по умолчанию для iOS в Makefile).

### 3.7. Требования к toolchain
- Для iOS: clang с `-arch arm64`, sysroot `iphoneos`, `TARGET_SYS=iOS`, `TARGET_LJARCH=arm64`.
- Host build требует gcc/clang.

## 4. Ограничения iOS

- Динамическая загрузка `.so`/`.dylib` ограничена code signing/sandbox; `ffi.load` на произвольные библиотеки не гарантирован.
- Статическая линковка — рекомендуемый путь.
- Символы должны быть доступны через статическую линковку или bridge.
- JIT запрещён на iOS (без `LUAJIT_ENABLE_JIT` и специальных прав).
- FFI-вызовы к системным C-функциям возможны (статически/системно доступные).
- Objective-C и системные API вызываются через C-bridge (не из Lua напрямую).
- Android ELF не может быть загружен на iOS.

## 5. Сравнение Lua 5.1 vs LuaJIT

| Критерий | Lua 5.1 | LuaJIT 2.1 (interpreter-only) | LuaJIT 2.1 (JIT, Android) |
|---|---|---|---|
| Совместимость с NeoMLoader | Высокая (5.1 API) | Высокая (5.1 compat) | Высокая |
| FFI | Нет (нужна отдельная libffi) | Да (встроенный) | Да |
| bit | Нет (нужен bitlib) | Да | Да |
| Производительность | Низкая | Средняя | Высокая |
| Размер | Малый | Средний | Средний |
| ARM64 | Да | Да | Да |
| iOS | Да | Да (interpreter) | Нет (JIT запрещён) |
| Сложность сборки | Низкая | Средняя | Средняя |
| Сложность интеграции | Средняя | Средняя | Высокая (JIT-права) |
| Риски | Отсутствие FFI | FFI-совместимость символов | JIT на iOS |

## 6. Результаты проверки сборки

### 6.1. Linux host build
- Команда: `make -j4` (и с `XCFLAGS="-DLUAJIT_DISABLE_JIT"`).
- Результат: **успешно** — `libluajit.a`, `luajit` собраны.
- FFI-тест: **passed** (структура, поля, вызов `puts`).
- JIT-disabled тест: `jit.status()` → "no jit", FFI работает.

### 6.2. iOS ARM64 build
- **Не выполнен** — в среде отсутствует Xcode/iOS SDK.
- Не заявляется успешным.
- Подготовлены инструкции (раздел 7).

## 7. Инструкции сборки LuaJIT для iOS ARM64 (interpreter-only)

```bash
# В отдельной директории, не в репозитории ArizonaNeo-iOS
git clone https://github.com/LuaJIT/LuaJIT.git luajit
cd luajit

# Static library, iOS, arm64, FFI enabled, JIT disabled
make -j4 \
  CROSS=... \
  TARGET_SYS=iOS \
  TARGET_LJARCH=arm64 \
  BUILDMODE=static \
  XCFLAGS="-DLUAJIT_DISABLE_JIT" \
  TARGET_CC="clang -arch arm64 -isysroot <iphoneos_sdk> -miphoneos-version-min=15.0"
```

Проверка FFI:
```lua
local ffi = require("ffi")
ffi.cdef[[ typedef struct { int x; } T; ]]
local t = ffi.new("T"); t.x = 7; assert(t.x == 7)
print("FFI OK")
```

## 8. Архитектура интеграции (предлагаемая)

- `LuaRuntime` — владеет `lua_State*` (через `luaL_newstate`/`lua_close`), настраивает allocator, открывает стандартные библиотеки, устанавливает FFI policy.
- `LuaHost` (существующий интерфейс) — реализуется `LuaJITHost`, использует `LuaRuntime`.
- `lua_State` ownership — RAII в `LuaRuntime`, без глобального состояния.
- Allocator — стандартный `lua_Alloc` или кастомный (для диагностики).
- Error handling — `lua_pcall`/`luaL_loadbuffer`, reporting через Logger.
- Module loader — собственный `package.searcher`/`require`, использующий `SandboxedFilesystem` и `package.loaded`.
- `package.path` — пути внутри sandbox.
- Sandbox — ограничение `io`/`os`/`debug` через обёртки.
- C API bindings — регистрация функций Core (Logger, Scheduler, EventBus) в Lua.
- FFI policy — разрешить cdef/new/cast/string/copy/fill/metatype/gc; запретить `ffi.load` произвольных библиотек (или разрешить только статически связанные).
- ScriptManager integration — `run()` вызывает `LuaRuntime::execute`, возвращает результат; состояние переходит в Running только при успешном запуске.
- Logger integration — перенаправление `print` и ошибок Lua в Logger.
- Scheduler integration — таймеры Lua через Scheduler (tick-driven).
- EventBus integration — события NeoLoader через EventBus.

Подключение к существующему Core:
- Не переписывать Logger/FS/Scheduler/EventBus/ScriptManager.
- Добавить `LuaRuntime` как новый компонент, зависящий от Core.
- `LuaJITHost` реализует `LuaHost`.

## 9. Риски и ограничения

- FFI на iOS может не иметь доступа к произвольным символам; `ffi.load("GTASA"/"monetloader")` не работает.
- `ffi.C.*` к системным функциям зависит от линковки.
- JIT отключён → производительность ниже, но приемлемо для скриптов.
- `io`/`os` требуют sandbox-адаптеров.
- `debug` библиотека может быть ограничена.
- Лицензия LuaJIT: MIT (проверить перед подключением).
- LuaJIT исходники не добавляются в Git (внешняя зависимость, собирается отдельно).

## 10. Рекомендация

**Использовать LuaJIT 2.1 в interpreter-only режиме с FFI enabled, собранную как статическая библиотека под iOS ARM64, с собственным module loader и sandbox.**

Обоснование:
- Сохраняет FFI (критично для NeoLoader-скриптов).
- Совместим с Lua 5.1 API (setfenv и др.).
- Поддерживает ARM64 и iOS (JIT выключен по умолчанию).
- FFI проверен в interpreter-only режиме (Linux).
- Статическая сборка поддерживается Makefile.

Следующий этап: интеграция LuaJIT в проект (Stage 7) — отдельная задача, требующая Apple toolchain.

## 11. Нерешённые вопросы

1. Доступность Apple toolchain для реальной iOS-сборки.
2. Политика FFI: какие символы разрешать.
3. Sandbox для `io`/`os` (пути).
4. Лицензия LuaJIT (проверить файл COPYRIGHT).
5. `debug` библиотека — ограничить?
6. Производительность interpreter-only на целевых скриптах.
7. Интеграция с `thisScript`/`script.this` (NeoLoader глобальные).
8. `lua_thread`/`wait` — реализация через Scheduler.

## 13. Stage 6.1 — iOS ARM64 build validation

Подготовлены `scripts/build_luajit_ios.sh` и `.github/workflows/luajit-ios.yml`.
Документ: `docs/LUAJIT_IOS_BUILD.md`.
Реальная iOS-сборка выполнена через GitHub Actions (run 45253149431): успех, `libluajit.a` 497K, Mach-O ARM64.
Linux-проверка FFI пройдена. iOS runtime FFI не проверялся.

`docs/PORTING_PLAN.md` обновлён: Stage 6.0 — исследование Lua Runtime (LuaJIT 2.1 feasibility), следующий этап — интеграция (Stage 7).
