# Stage 6.2.4 — LuaJIT Runtime Validation and Hardening

Дата: 2026-11-24. Окружение: Linux AArch64 (PRoot Ubuntu поверх Termux), GCC 15.2.0, binutils 2.46, CMake 4.5.

## 1. LuaJIT commit и конфигурация

- Пinned commit (не менялся): `96f7b297326e9fd454b7186c6858b2773f94a0dc`.
- Версия: LuaJIT 2.1.1795119671, Lua 5.1 API (`lua.h` / `luajit.h`).
- ARCH: `aarch64-linux-gnu`; 141 объект архива — ARM aarch64; iOS-артефакты не смешаны.
- Добавлен воспроизводимый скрипт `scripts/build_luajit_linux_host.sh`:
  проверяет commit и целевую платформу, выполняет
  `make BUILDMODE=mixed XCFLAGS="-DLUAJIT_DISABLE_JIT"`, проверяет символы
  C API/`luaopen_ffi` и поведенчески проверяет JIT/FFI через `./luajit`.

### 1.1. Коррекция отчёта Stage 6.2.3

Stage 6.2.3 сделал вывод «JIT включён» по закомментированной строке
`#XCFLAGS+= -DLUAJIT_DISABLE_JIT` в Makefile. Это было **неверно**: Makefile
показывал значение по умолчанию, а не фактические флаги собранного архива. Stage 6.2.4 доказал обратное двумя независимыми способами:

1. Поведенческий тест: `jit.status()` → `false`, `select('#', jit.status())` → `1`
   (при JIT-enabled функция возвращает `true` и строки CPU/оптимизаций).
2. Пересборка того же commit с явным `XCFLAGS="-DLUAJIT_DISABLE_JIT"` в
   `.luajit-stage621/LuaJIT-nojit` дала **байт-идентичный** архив:

```
963c7ea3e577b21d09e039837f1a9a87a0ab4220fab5f9430a2985908e70eab8  LuaJIT/src/libluajit.a
963c7ea3e577b21d09e039837f1a9a87a0ab4220fab5f9430a2985908e70eab8  LuaJIT-nojit/src/libluajit.a
720c733304681f741fb547195368b93267a2ce223057a3039d4ed58b47bccb0a  LuaJIT/src/libluajit.so  (аналогично)
```

Вывод: **JIT был отключён и в исходной сборке**; конфигурация теперь зафиксирована
в скрипте. Соответствующий пункт `STAGE_6_2_3_LINKER_DIAGNOSIS.md` исправлен.

### 1.2. JIT disabled / FFI enabled — доказательства

- `LJ_HASJIT = 0` при `defined(LUAJIT_DISABLE_JIT)` (lj_arch.h:611); `LJ_HASFFI = 1`
  (нет `LUAJIT_DISABLE_FFI`, `LJ_ARCH_NOFFI` для ARM64 не определён).
- `nm -g --defined-only libluajit.a`: `luaL_newstate`, `lua_pcall`, `lua_close`,
  `luaopen_jit`, `luaopen_ffi` — все `T`.
- Runtime (через `./luajit` и через интегрированный `LuaJITHost`):
  `jit.status() == false`, `select('#', jit.status()) == 1`,
  `require('ffi')` + `ffi.new` + чтение/запись полей структуры работают.
- Заголовки и библиотека одной сборки: `luaJIT_version_2_1_1795119671`
  соответствует `luajit.h:35`.

## 2. Аудит состояния перед изменениями (факты)

| Вопрос | Фактическое состояние до Stage 6.2.4 |
|---|---|
| Библиотеки | base, math, string, table, package, ffi, jit через `lua_pcall` + `lua_setglobal`; io/os/debug не открываются |
| lua_State | `luaL_newstate()` в `initialize()`; `lua_close()` в `shutdown()`/деструкторе |
| Compile error | `luaL_loadbuffer != 0` → `last_error_` из error-объекта → `lua_settop(0)` → false |
| Runtime error | `lua_pcall != 0` → то же; false |
| Стек после execute | `lua_settop(state_, 0)` в success-пути и в catch-блоках |
| Повторный initialize() | early return `true` (утечки нет), но **теста не было** |
| execute() до/после shutdown | false + `"runtime not initialized"`; после shutdown тест был, до — нет |
| longjmp | Защищены только `luaL_loadbuffer`/`lua_pcall`; `lua_getglobal`, `lua_tolstring(error)`, `lua_close(__gc)` — не защищены; C++ try/catch longjmp не ловит |
| Тесты | init, execute, globals, syntax/runtime errors, shutdown, re-init, FFI; JIT-тест использовал global `jit` без require; реальной проверки stack_top, metamethod, wrong-type, FFI-error не было |

## 3. Изменения в коде и их обоснование

### 3.1. `src/runtime/lua_jit_host.cpp`

1. **Protected getter trampoline** `protected_getglobal()` + `lua_pcall()` в
   `get_global_number()` / `get_global_string()`. Проблема: `lua_getglobal()`
   может вызвать `__index`-метамethod таблицы `_G`; `error()` внутри него — это
   longjmp через C++-фреймы с живым `std::string` (неопределённое поведение).
   Теперь longjmp превращается в код ошибки `lua_pcall`, стек очищается.
2. **`copy_error_message()` с проверкой `lua_type()`** (только `LUA_TSTRING` /
   `LUA_TNUMBER`) вместо прямого `lua_tolstring()` на произвольном error-объекте:
   исключает вызов metamethod и longjmp при извлечении сообщения.
3. **Явная очистка стека** в error-путях геттеров и в `copy_error_message()`.
4. **Комментарии про `lua_close`**: LuaJIT вызывает финализаторы через защищённый
   `lj_vm_pcall` (lj_gc.c:522), но произвольный Lua `__gc`-код может менять
   состояние; `shutdown()` вызывается без живых C++-локальных объектов с
   нетривиальными деструкторами.
5. Оставлен существующий `extern "C"`-wrapper (исправление Stage 6.2.3).

### 3.2. `include/arizona_neo/runtime/lua_jit_host.hpp`

- Добавлен `int stack_top() const noexcept` — минимальный тестовый API для
  реальной проверки очистки Lua-стека (ранее непроверяемой). Поведение runtime
  не меняет.

### 3.3. `tests/lua_runtime_test.cpp`

Расширен с 14 сценариев до покрытия всех required-пунктов: lifecycle, выполнение,
globals, metamethod-longjmp, JIT, FFI (включая ошибку), shutdown/re-init.
Результат `return 2+2` **не** выдаётся за C++ return value — значения читаются
через globals.

### 3.4. Прочее

- `scripts/build_luajit_linux_host.sh` — воспроизводимая сборка/проверка.
- `.gitignore` — добавлен `.luajit-stage621/` (артефакты не попадут в Git).
- Документы обновлены; в `STAGE_6_2_3_LINKER_DIAGNOSIS.md` исправлен ошибочный вывод про JIT.

## 4. Сборки и тесты

Все configure выполнялись с `export PREFIX=/data/data/com.termux/files/usr`
(иначе Termux-CMake 4.5 падает на `$ENV{PREFIX}/include/android/api-level.h`).

| Конфигурация | Каталог | Configure | Build | CTest |
|---|---|---|---|---|
| Debug + LuaJIT | `build/stage624_debug` | exit 0 | exit 0 | **6/6 Passed** |
| Release + LuaJIT | `build/stage624_release` | exit 0 | exit 0 | **6/6 Passed** |
| Debug без LuaJIT | `build/stage624_nojit_debug` | exit 0 | exit 0 | 6/6 Passed (lua_runtime_test SKIPPED) |
| Release без LuaJIT | `build/stage624_nojit_release` | exit 0 | exit 0 | 6/6 Passed |

Полные логи: `/tmp/opencode/stage624-{debug,release}-{configure,build,ctest}.log`,
`/tmp/opencode/stage624-nojit-{Debug,Release}-*.log`.

`lua_runtime_test` действительно линкован с LuaJIT (не заглушка):
```
nm build/stage624_debug/tests/lua_runtime_test | grep ' T '
  luaJIT_version_2_1_1795119671
  luaL_newstate
  luaopen_ffi
  luaopen_jit
```
Прямой запуск: `lua runtime tests: PASSED`, exit 0.

## 5. Runtime execution results

- `return 2 + 2` → execute true, стек пуст; значение проверено отдельно через
  `result_value = 2 + 2` → `get_global_number == 4.0`.
- `local value = 123 assert(...)` → true, global == 123.
- `local text = "LuaJIT runtime works" assert(#text>0)` → true, global == строка.
- Три последовательных chunk с общим состоянием (`a=1`, `b=a+1`, `c=b*10`) → `c == 20`.
- Синтаксическая ошибка → false, сообщение непустое, стек пуст.
- `error('boom')` → false, сообщение содержит `boom`, стек пуст.
- Восстановление после ошибок → следующий execute true, globals доступны, `last_error_` очищена.

## 6. FFI runtime results

Через интегрированный `LuaJITHost`: `require('ffi')` доступен; `ffi.cdef` с
`TestPoint { int x; int y; }`; `ffi.new`, запись/чтение полей → 10 и 20;
`assert` в FFI-скрипте прошли. Ошибка FFI (`ffi.new('ThisTypeDoesNotExist')`)
→ execute false, сообщение в `last_error()`, стек пуст; следующий execute
успешен, состояние Lua не разрушено. `ffi.load` не используются.

## 7. Lifecycle и error handling

- Повторный `initialize()` на живом state → true, состояние (global `persistent`) сохранено,
  второго `lua_State` не создаётся.
- `shutdown()` → `is_initialized() == false`, стек пуст; повторный `shutdown()` — no-op.
- `execute()` после shutdown → false + непустое `last_error()`.
- Re-init после shutdown → true, свежий state (старые globals исчезли), execute работает.
- `__index`-метамethod `_G`, бросающий `error()`: `get_global_number()` вернул 0.0
  без аварийного завершения, стек пуст; после снятия метамethod'а state работоспособен.
  **До укрепления этот сценарий приводил бы к longjmp через C++-деструкторы (UB).**
- Не-string error object обрабатывается fallback-сообщением.

## 8. ASan / LSan — НЕ ВЫПОЛНЕНО (environment blocker)

ASan-сборка (`build/stage624_asan`, `-fsanitize=address`) и LSan-сборка
(`build/stage624_lsan`, `-fsanitize=leak`) **скомпилировались**, но не запустились:

```
==20591==Error: heap size 40000002000 exceeds max user virtual address 7fffffffff
==20591==Try using a kernel that allows a larger virtual address space
==20591==ERROR: AddressSanitizer: out of memory: failed to allocate ...
```

LSan: тот же `heap size 40000002000 exceeds max user virtual address` и
`LeakSanitizer: CHECK failed: sanitizer_allocator_primary64.h:149`.

Причина: PRoot-ядро не предоставляет ASan/LSan фиксированное 4 ТБ виртуальное
адресное пространство для shadow memory. Это **не** успешная проверка: use-after-free,
double-close, stack corruption и leaks инструментально **не проверены**; доступны
только функциональные тесты lifecycle/stack/errors выше.

## 9. Lua longjmp safety analysis

| API | Риск | Обработка |
|---|---|---|
| `luaL_loadbuffer`, `lua_pcall` | protected по дизайну | коды ошибок → `last_error_`, `lua_settop(0)` |
| `lua_getglobal` (геттеры) | `__index` metamethod → longjmp | вызван внутри C-trampoline под `lua_pcall` |
| `lua_tolstring` error-объект | metamethod-конверсия | проверка `lua_type`, только string/number |
| `lua_gettop` | безопасен | используется в `stack_top()` |
| `lua_close` | `__gc`-финализаторы | LuaJIT защищает их внутренним `lj_vm_pcall`; вызывать без живых C++-объектов; побочные эффекты финализаторов не гарантированы |
| `lua_pushcfunction`/`lua_pushstring` ( unprotected ) | OOM → longjmp | остаточный риск при исчерпании памяти; C++ try/catch его не ловит — документировано, не устранено в этом этапе |

## 10. iOS readiness (только статический анализ)

- using-API: `luaL_newstate`, `lua_pcall`, `luaL_loadbuffer`, `lua_getglobal`,
  `lua_isnumber`, `lua_isstring`, `lua_tonumber`, `lua_tolstring`, `lua_settop`,
  `lua_gettop`, `lua_pushcfunction`, `lua_pushlightuserdata`, `lua_pushstring`,
  `lua_close`, `luaopen_*` — все входят в LuaJIT 2.1 / Lua 5.1 API, доступны в iOS ARM64-сборке.
- В `src/runtime/` и заголовке **нет** `dlopen/dlsym/unistd/pthread/JNI/android/bionic/__linux`.
- C++17 (`CMAKE_CXX_STANDARD 17`, `cxx_std_17`) — без платформенных расширений.
- `scripts/build_luajit_ios.sh` использует `TARGET_SYS=iOS`, `TARGET_LJARCH=arm64`,
  `-miphoneos-version-min=15.0`, `XCFLAGS="-DLUAJIT_DISABLE_JIT"`, без `LUAJIT_DISABLE_FFI`.
- Linux-хост линкует `m` (доступен и на iOS через libSystem); `dl` в основном
  CMake не добавлялся; linker hacks/`--whole-archive` не используются.
- **Не проверено**: реальная работа LuaJIT/FFI на iOS-устройстве, `ffi.load` на iOS
  (dylib-загрузка restrictions). Полной iOS-интеграции не заявляется.

## 11. Оставшиеся ограничения / blockers

1. ASan/LSan недоступны в PRoot (точная причина выше) — инструментальная проверка
   памяти не выполнена.
2. iOS ARM64 артефакт Stage 6.1.3 не проверен на macOS/устройстве в этом этапе.
3. OOM-longjmp в unprotected Lua API (`lua_pushcfunction`/`lua_pushstring`) остаётся
   теоретическим риском; устранение потребует protects alloc-пути или кастомного
   аллокатора с error-обработкой (out of scope).
4. Lua `__gc` при `lua_close` может выполнять произвольный код; финализаторы с
   ошибками не должны использоваться для критичных побочных эффектов.

Stage 6.2.4 не переходит к Stage 6.3: критерии в части Debug/Release/CTest/execution/
FFI/JIT/lifecycle/error-handling выполнены; ASan/LSan зафиксирован какenvironment
blocker, iOS — как непроверенный этап.
