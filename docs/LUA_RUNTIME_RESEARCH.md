# Lua Runtime Research (Stage 6.0 / 6.2)

Дата: 2026-11-23.
Репозиторий: `/root/ArizonaNeo-iOS`.
Исследовательские материалы: `/root/ArizonaNeo-research` (NeoLoader 1.0.72 build 172). LuaJIT исходники не хранятся в Git.

## 1. Объём анализа (Stage 6.0, без изменений)

- Изучено `.lua` файлов: **210** (включая корневые скрипты, `lib/`, подкаталоги).
- Метод: статическое чтение (`cat`/`sed`/`grep`), без выполнения скриптов.
- Подробный инвентарь API и таблицы см. в истории этого документа (Stage 6.0). Ключевые выводы: массовое использование `ffi`, `bit`, `jit.*`, `io`/`os`, `require`, Lua 5.1-совместимость (`setfenv` встречается).

## 2. LuaJIT 2.1 (закреплено)

- Repository: `https://github.com/LuaJIT/LuaJIT`
- Commit: `96f7b297326e9fd454b7186c6858b2773f94a0dc` (LuaJIT 2.1, Lua 5.1 API, MIT)
- iOS ARM64 reference (Stage 6.1.3): GitHub Actions run `45253149431`, artifact `luajit-ios-arm64`, `libluajit.a` (~497K, Mach-O arm64). iOS runtime не тестировался, только compile/link в CI context.

## 3. Stage 6.2 — интеграция в Core

- Реализация: `arizona_neo::runtime::LuaJITHost` (см. `include/arizona_neo/runtime/lua_jit_host.hpp`, `src/runtime/lua_jit_host.cpp`), реализует `arizona_neo::core::LuaHost`.
- CMake: опция `ARIZONA_NEO_WITH_LUAJIT` (по умолчанию OFF). При включении требуются `LUAJIT_INCLUDE_DIR`/`LUAJIT_LIBRARY` (или `LUAJIT_ROOT`). Определяет `ARIZONA_NEO_WITH_LUAJIT=1`, линкует статическую библиотеку LuaJIT.
- Жизненный цикл: `initialize()` создаёт `lua_State`, открывает минимальный набор библиотек (`_G`, `math`, `string`, `table`, `package`, `ffi`, `jit`), `shutdown()` закрывает состояние (RAII в деструкторе).
- JIT: остаётся отключённым политикой сборки; тесты проверяют `jit.status()` при включённом рантайме.
- FFI: включён в референсной iOS-сборке; на host-тестах проверяется базовое использование `ffi.new`/cdef. iOS device runtime FFI не проверялся.
- Тесты: `tests/lua_runtime_test.cpp` (пропуск, если LuaJIT выключен).

Детали: `docs/LUAJIT_RUNTIME_INTEGRATION.md`, `docs/LUAJIT_IOS_BUILD.md`.

## 4. Ограничения (актуально)

- Не использовать Linux/macOS/iOS архивы взаимозаменяемо.
- Не хранить LuaJIT sources/binaries в Git.
- На этом этапе не реализованы: require loader, package.path resolver, NeoLoader/moonloader API, Android JNI/Bionic, GTASA bindings, GameBridge, Script Manager UI.
- `ffi.load` произвольных библиотек и расширенный sandbox — отдельные этапы; текущее открытие `package`/`ffi`/`jit` задокументировано как development-oriented минимальный набор для Stage 6.2.

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

## 14. Stage 6.2.4 — фактическая валидация runtime (Linux AArch64 host)

- Пinned commit `96f7b297326e9fd454b7186c6858b2773f94a0dc`; JIT отключён и FFI включён подтверждены поведенчески (`jit.status() == false`, `select('#', ...) == 1`; `require('ffi')` + struct-поля) и байт-идентичной пересборкой с `XCFLAGS="-DLUAJIT_DISABLE_JIT"` (sha256 `963c7ea3…`). Воспроизводимая команда: `scripts/build_luajit_linux_host.sh`.
- Ранее сделанный в Stage 6.2.3 вывод «JIT включён» (по закомментированному define в Makefile) признан ошибочным и исправлен в `docs/STAGE_6_2_3_LINKER_DIAGNOSIS.md`.
- Интегрированный `LuaJITHost` проверен в Debug и Release: сборки OK, ctest 6/6 в обеих конфигурациях; JS-выполнение, globals, ошибки, lifecycle, JIT/FFI-сценарии проходят; реальная очистка Lua-стека проверена через `stack_top()`.
- Без LuaJIT (`ARIZONA_NEO_WITH_LUAJIT=OFF`) Debug и Release собираются, 6/6 тестов проходят (`lua_runtime_test` SKIPPED).
- Ограничения: ASan/LSan не запускаются в PRoot из-за VA-лимита shadow memory (`heap size 40000002000 exceeds max user virtual address`); iOS ARM64 runtime и `ffi.load` на устройстве не проверялись. Подробности: `docs/STAGE_6_2_4_RUNTIME_VALIDATION.md`.
