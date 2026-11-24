# Stage 6.2.3 — LuaJIT Linker Isolation and ABI Diagnosis

Дата: 2026-11-23/24
Рабочая директория: `~/ArizonaNeo-iOS`
LuaJIT дерево: `.luajit-stage621/LuaJIT` (не изменялось: commit зафиксирован, объекты не тронуты)
Изоляция тестов: `.stage623-link-test/` (исключён из Git через `.gitignore`)

---

## 1. Toolchain versions

| Компонент | Версия |
|---|---|
| Ядро | `Linux localhost 6.17.0-PRoot-Distro #1 SMP PREEMPT_DYNAMIC aarch64 GNU/Linux` (PRoot-дистрибутив Ubuntu поверх Android/Termux) |
| gcc | `gcc (Ubuntu 15.2.0-16ubuntu1) 15.2.0` |
| g++ | `g++ (Ubuntu 15.2.0-16ubuntu1) 15.2.0` |
| ld | `GNU ld (GNU Binutils for Ubuntu) 2.46` |
| ar | `GNU ar (GNU Binutils for Ubuntu) 2.46` |
| nm | `GNU nm (GNU Binutils for Ubuntu) 2.46` |
| CMake | CMake 4.5 (Termux-сборка, `/data/data/com.termux/files/usr/share/cmake-4.5`) |

## 2. Architecture

- `gcc -dumpmachine` → `aarch64-linux-gnu`
- `g++ -dumpmachine` → `aarch64-linux-gnu`
- Хост и целевая архитектура совпадают (нативная сборка Linux AArch64).

## 3. LuaJIT commit

```
96f7b297326e9fd454b7186c6858b2773f94a0dc  (Merge branch 'master' into v2.1)
LUAJIT_VERSION      "LuaJIT 2.1.1795119671"   (src/luajit.h:33)
LUAJIT_VERSION_NUM  20199
LUA_VERSION         "Lua 5.1", LUA_VERSION_NUM 501
```

Commit в ходе этапа не менялся. Дерево `.luajit-stage621` не удалялось и не пересобиралось.

## 4. Archive and shared library information

```
libluajit.a:  current ar archive (620014 байт)
libluajit.so: ELF 64-bit LSB shared object, ARM aarch64, dynamically linked, stripped
```

Состав архива: **141 объект, все — `ELF 64-bit LSB relocatable, ARM aarch64`**.
Смешения архитектур нет, iOS-артефактов в Linux-сборке нет, Android-объектов нет.

## 5. Symbol inspection

Определения в статическом архиве (все глобальные, текстовые, без visibility-ограничений):

```
lib_aux.o:  T luaL_newstate
lj_state.o: T lua_close
lj_api.o:   T lua_getfield
lj_api.o:   T lua_pcall
```

Динамические символы `libluajit.so` (readelf -Ws, секция .dynsym):

```
165: 00000000000373b0  136 FUNC GLOBAL DEFAULT  11 luaL_newstate
174: 00000000000131a0  172 FUNC GLOBAL DEFAULT  11 lua_close
102: 000000000001bae0  180 FUNC GLOBAL DEFAULT  11 lua_getfield
145: 000000000001c680  228 FUNC GLOBAL DEFAULT  11 lua_pcall
```

Вывод о visibility делался по `readelf -Ws`, а не только по `nm`:
**hidden visibility отсутствует** — все четыре функции `GLOBAL DEFAULT`.

Undefined symbols архива — ожидаемые: функции libm (`cos`, `pow`, …), `_Unwind_*`
из libgcc, внутренние `lj_*`. Аномалий нет.

Прочие проверенные символы: `luaopen_base`, `luaopen_jit`, `luaopen_ffi`,
`luaJIT_version_2_1_1795119671` — присутствуют в архиве (T).

## 6. C minimal test result

`main.c` (чистый C): **успех**.

```
gcc -I../.luajit-stage621/LuaJIT/src -c main.c -o main_c.o          → OK
gcc main_c.o ../.luajit-stage621/LuaJIT/src/libluajit.a -lm -ldl \
    -o lua_link_test_c                                              → exit 0
./lua_link_test_c                                                   → exit 0
```

`nm -u main_c.o`: `luaL_newstate`, `lua_close`, `lua_pushnumber`, `lua_tonumber`
(неманглённые C-имена).

## 7. C++ minimal test result

### 7.1. С обёрткой `extern "C"` (как в задании) — успех

```
g++ -I../.luajit-stage621/LuaJIT/src -c main.cpp -o main.o           → OK
g++ main.o ../.luajit-stage621/LuaJIT/src/libluajit.a -lm -ldl \
    -o lua_link_test                                                 → exit 0
./lua_link_test                                                      → exit 0
```

`nm -u main.o`: те же неманглённые C-имена, `_Z*` отсутствуют.

### 7.2. Без обёртки `extern "C"` — воспроизведённый отказ

`main_noextern.cpp` — идентичный код, но включения заголовков без `extern "C"`:

```
g++ -c main_noextern.cpp -o main_noextern.o                          → OK
nm -u main_noextern.o:
    U _Z13luaL_newstatev
    U _Z9lua_closeP9lua_State
    U _Z14lua_pushnumberP9lua_Stated
    U _Z12lua_tonumberP9lua_Statei
g++ main_noextern.o ../.luajit-stage621/LuaJIT/src/libluajit.a -lm -ldl \
    -o lua_link_test_noextern                                        → exit 1
```

Точная ошибка линкера:

```
/usr/bin/aarch64-linux-gnu-ld.bfd: main_noextern.o: in function `main':
main_noextern.cpp:(.text+0x8): undefined reference to `luaL_newstate()'
main_noextern.cpp:(.text+0x2c): undefined reference to `lua_pushnumber(lua_State*, double)'
main_noextern.cpp:(.text+0x38): undefined reference to `lua_tonumber(lua_State*, int)'
main_noextern.cpp:(.text+0x60): undefined reference to `lua_close(lua_State*)'
collect2: error: ld returned 1 exit status
```

Причина: заголовки LuaJIT **не содержат собственных `extern "C"` guards**
(см. раздел 12), поэтому C++ компилятор манглят имена, а в архиве лежат только
C-имена.

## 8. Static linking result

| Вариант | Линковка | Запуск |
|---|---|---|
| C++, static, `extern "C"` | ✅ exit 0 | ✅ exit 0 |
| C, static | ✅ exit 0 | ✅ exit 0 |
| C++, static, без `extern "C"` | ❌ exit 1 (undefined refs) | — |

Порядок аргументов (`main.o` перед архивом) корректен; перестановки и
`--whole-archive`/`--start-group` не потребовались.

## 9. Shared linking result

Первая попытка: линковка прошла, но `ldd` — `libluajit-5.1.so.2 => not found`.
RUNPATH был записан корректно; отсутствовал файл с именем soname: в `src/` есть
только `libluajit.so` (soname-симлинки обычно создаёт `make install`, а не `make`).

Созданы симлинки (как это делает LuaJIT Makefile):

```
ln -sf libluajit.so libluajit-5.1.so.2
ln -sf libluajit-5.1.so.2 libluajit-5.1.so
```

После этого:

```
ldd lua_link_test_shared → libluajit-5.1.so.2 => .../src/libluajit-5.1.so.2 ✅
./lua_link_test_shared   → exit 0
```

Для основного проекта это неактуально: используется статический `libluajit.a`.

## 10. CMake minimal test result

`CMakeLists.txt` в `.stage623-link-test/`: минимальный проект, включает заголовки
LuaJIT, линкует конкретный `libluajit.a` + `m` + `dl`, создаёт один executable,
CTest запускает его.

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug  → OK (см. раздел 13 про PREFIX)
cmake --build build                           → OK
ctest                                         → 1/1 Passed (lua_link_test_runs)
```

`--whole-archive`, `--start-group` и иные обходы не использовались.

## 11. Exact linker errors

Полный текст ошибки — в разделе 7.2. Кратко:

```
undefined reference to `luaL_newstate()'
undefined reference to `lua_pushnumber(lua_State*, double)'
undefined reference to `lua_tonumber(lua_State*, int)'
undefined reference to `lua_close(lua_State*)'
```

Это C++-манглинг (`_Z13luaL_newstatev`, `_Z9lua_closeP9lua_State`, …), который
не может разрешиться против C-символов архива.

## 12. Root cause

### Основная причина (линковка)

**Заголовки LuaJIT не имеют `extern "C"` guards, а `src/runtime/lua_jit_host.cpp`
включал их без обёртки.**

Доказательства:

1. `grep -n 'extern "C"' src/lua.h src/luaconf.h src/lualib.h src/luajit.h src/lauxlib.h`
   — **ни одного совпадения**. `LUA_API` определён как просто `extern`
   (luaconf.h:138; при visibility-сборке — `extern __attribute__((visibility("default")))`,
   luaconf.h:136), без `#ifdef __cplusplus`.
2. Минимальный тест без обёртки воспроизводит отказ (раздел 7.2) — те же
   `undefined reference to luaL_newstate()`, что были в основном проекте.
3. Минимальный тест с обёрткой линкуется и работает (раздел 7.1).

То есть проблема **не** в: порядке линковки, видимости символов, архитектуре
объектов, смешении iOS/Linux артефактов, отсутствующих зависимостях, FFI/JIT
конфигурации. Проблема — в C++ name mangling из-за отсутствия `extern "C"`.

### Дополнительные находки (окружение, не LuaJIT)

1. **CMake configure в Termux/PRoot-окружении.** Termux-сборка CMake 4.5
   определяет host system как `Android` и в `CMakeDetermineSystem.cmake:40`
   читает `$ENV{PREFIX}/include/android/api-level.h`. В PRoot-Ubuntu `PREFIX`
   пуст → `file(READ /include/android/api-level.h)` → **фатальная ошибка
   configure на любом свежем каталоге сборки**. Обходной путь:
   `PREFIX=/data/data/com.termux/files/usr cmake ...` (файл существует в
   Termux-префиксе). Повторный configure в том же каталоге проходит из-за
   кэша. Перекрытие `-DCMAKE_HOST_SYSTEM_NAME=Linux` не помогает.
   На GitHub Actions CI (настоящий Linux) проблема не воспроизводится.
2. **Soname-симлинки для shared-линковки** отсутствуют в `src/` (см. раздел 9).
   Влияния на основной проект нет.

### Проверка конфигурации LuaJIT (запрошенные пункты)

- `LJ_ARCH_NOFFI`: найден в lj_arch.h:367, но внутри **PowerPC**-блока
  (`LJ_TARGET_PPC` / `LJ_ARCH_PPC32ON64`); к ARM64 не относится.
  FFI включён: `luaopen_ffi` определён в архиве.
- `LUAJIT_DISABLE_JIT`: в src/Makefile закомментирован (`#XCFLAGS+= -DLUAJIT_DISABLE_JIT`),
  но это **не отражает фактическую конфигурацию собранного архива**. Stage 6.2.4
  доказал поведенчески (`jit.status() == false`, `select('#', ...) == 1`) и
  байт-идентичной пересборкой с `XCFLAGS="-DLUAJIT_DISABLE_JIT"` (sha256
  `963c7ea3…`), что JIT в архиве **уже отключён**. Вывод об «включённом JIT»,
  сделанный по Makefile в этом отчёте, был ошибочным и исправлен.
- Архив и заголовки одной сборки: версия символа
  `luaJIT_version_2_1_1795119671` совпадает с `luajit.h:35`.
- Архитектура объектов: 141/141 — ARM aarch64, смешения нет.
- Смешения iOS и Linux артефактов нет.

## 13. Recommended fix (и что применено)

### Применено (минимальное изменение)

`src/runtime/lua_jit_host.cpp` — обёртка включений LuaJIT в `extern "C"`:

```cpp
// LuaJIT headers (unlike many other C libraries) do not contain their own
// `extern "C"` guards, so they MUST be wrapped here when compiled as C++.
// Without this, the C++ compiler mangles Lua API names (e.g. _Z13luaL_newstatev)
// and the linker cannot resolve them against libluajit.a (Stage 6.2.3).
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
```

Обёртка добавлена в потребителе, поскольку заголовки guards не содержат
(добавлять «вторые» объявления API не потребовалось). Другие файлы проекта
Lua-заголовки не включают. Основной `CMakeLists.txt` не менялся: причина была
не в порядке линковки.

### Рекомендации

1. Зафиксировать в `docs/LUAJIT_RUNTIME_INTEGRATION.md` правило: все будущие
   C++ TU, включающие заголовки Lua/LuaJIT, обязаны оборачивать их в
   `extern "C" {}`.
2. Для свежих configure в Termux/PRoot-среде использовать
   `PREFIX=/data/data/com.termux/files/usr` (зафиксировано в разделе 13.1 ниже);
   в CI обход не нужен.
3. Для shared-варианта LuaJIT (если когда-либо понадобится) — создавать
   soname-симлинки; основной проект продолжает использовать статический архив.

### 13.1. Воспроизведение

Точная причина (C++ mangling):

```
g++ -I.luajit-stage621/LuaJIT/src -c .stage623-link-test/main_noextern.cpp \
    -o /tmp/noextern.o
nm -u /tmp/noextern.o            # U _Z13luaL_newstatev ...
g++ /tmp/noextern.o .luajit-stage621/LuaJIT/src/libluajit.a -lm -ldl -o /tmp/t
# → undefined reference to `luaL_newstate()` и др., exit 1
```

Ошибка configure (окружение):

```
cmake -S . -B /tmp/fresh &&   # БЕЗ PREFIX
# CMake Error ... CMakeDetermineSystem.cmake:40 (file):
# file failed to open for reading: /include/android/api-level.h
PREFIX=/data/data/com.termux/files/usr cmake -S . -B /tmp/fresh   # → OK
```

## 14. Remaining blockers

1. **Окружение разработки**: свежий `cmake configure` в Termux/PRoot требует
   `PREFIX=/data/data/com.termux/files/usr` (или двойного запуска). Это
   неудобство локальной среды, не блокер для CI (GitHub Actions Linux не
   затронут).
2. **iOS-цель не проверена на macOS**: этот этап подтвердил линковку только для
   Linux AArch64 (host-инструментарий). Артефакт Stage 6.1.3
   (`luajit-ios-arm64`, `libluajit.a` для iOS ARM64) всё ещё требует проверки
   линковки через настоящий Apple toolchain (macOS/Xcode/CI с sysroot iOS).
3. Shared-линковка LuaJIT на iOS не применима (подпись dylib), целевой сценарий
   — статическая линковка; отдельно не блокирует.

## 15. Контрольные прогоны после исправления

- Минимальные тесты: C++ static exit 0, C static exit 0, C++ shared exit 0,
  повторная CMake-сборка + CTest 1/1.
- Основной проект (LuaJIT-enabled **Debug**, `build/luajit_debug`):
  сборка успешна, `lua_runtime_test` слинкован, **ctest 6/6 Passed**;
  повторная сборка идемпотентна; бинарник содержит реальные символы LuaJIT
  (`luaJIT_version_2_1_1795119671`, `luaopen_ffi`).
- Полностью свежий configure основного проекта (вне репозитория, с `PREFIX`)
  + сборка + тесты: **6/6 Passed**.

Интеграция в основной проект подтверждена запуском `lua_runtime_test`
(LuaJITHost: initialize → execute → проверка значений), а не только минимальным
executable.
