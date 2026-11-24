# LuaJIT runtime integration (Stage 6.2)

## 1. Scope
- Integrates LuaJIT 2.1 into C++ Core via `arizona_neo::runtime::LuaJITHost`, implementing `arizona_neo::core::LuaHost`.
- Goal: real Lua runtime with `lua_State` lifecycle, script execution, error handling, safe shutdown.
- Out of scope (separate stages): NeoLoader/moonloader API, require loader/package.path resolver, Android JNI/Bionic, GTASA/memory/SAMemory/SA-MP/ImGui/GameBridge, Script Manager UI.

## 2. Pinned LuaJIT
- Repository: `https://github.com/LuaJIT/LuaJIT`
- Commit: `96f7b297326e9fd454b7186c6858b2773f94a0dc` (LuaJIT 2.1, Lua 5.1 API, MIT)
- Build policy (Stage 6.1.3): JIT disabled, FFI enabled.
- iOS reference artifact: GitHub Actions run `45253149431`, artifact `luajit-ios-arm64`, `libluajit.a` (~497K, Mach-O arm64), built on macos-15-intel (iOS arm64). iOS runtime not tested.

## 3. LuaHost API (unchanged base contract)
Header: `include/arizona_neo/core/lua_host.hpp`

```cpp
class LuaHost {
public:
    virtual ~LuaHost() = default;
    virtual bool initialize() = 0;
    virtual bool execute(std::string_view source, std::string_view chunk_name) = 0;
    virtual void shutdown() noexcept = 0;
};
```

`LuaJITHost` (`include/arizona_neo/runtime/lua_jit_host.hpp`) extends behavior via non-virtual helpers (minimal extension, no base change):
- `const std::string& last_error() const noexcept`
- `bool is_initialized() const noexcept`
- `double get_global_number(const char*) const`
- `std::string get_global_string(const char*) const`
- `int stack_top() const noexcept` (тестовая поддержка: проверка очистки Lua-стека, добавлена в Stage 6.2.4)

## 4. Architecture (LuaJITHost)
- Files:
  - `include/arizona_neo/runtime/lua_jit_host.hpp`
  - `src/runtime/lua_jit_host.cpp` (compiled only when `ARIZONA_NEO_WITH_LUAJIT=1`)
- RAII: destructor calls `shutdown()`. `shutdown()` closes `lua_State` if non-null, clears `initialized_`.
- State: `lua_State* state_`, `bool initialized_`, `std::string last_error_`.
- Errors (Stage 6.2.4): load/pcall errors are copied only from string/number error objects; stack is cleared to top=0. C++ `try/catch` does **not** intercept Lua longjmp. Longjmp-prone paths are run through protected Lua C API calls: `luaL_loadbuffer`/`lua_pcall` for chunks and a C trampoline under `lua_pcall` for `lua_getglobal` in the global getters (`__index` metamethods can otherwise longjmp through live C++ objects).
- Execution: `luaL_loadbuffer` then `lua_pcall(state, 0, 1, 0)`; on any error returns `false` and sets `last_error_`; stack reset to empty after each call (verified via `stack_top()`).

## 5. Opened standard libraries (minimal set)
Opened in `initialize()` via `luaL_requiref`:
- `_G` (`luaopen_base`)
- `math`, `string`, `table`
- `package`
- `ffi`
- `jit`

Not opened at this stage: `io`, `os`, `debug` (kept out of the minimal surface). This is a **development-oriented minimal set** for Stage 6.2 validation; further sandboxing is out of scope.

**Constraints (documented):**
- Do not expose arbitrary device filesystem/system access goals in this stage.
- `ffi.load` of arbitrary dynamic libraries is not a supported/tested goal here; behavior is platform-dependent and must be treated carefully.
- iOS runtime FFI behavior is not validated on device in this stage (only compile/link of the static lib was validated in Stage 6.1.3 context).

## 6. CMake configuration
- Option: `ARIZONA_NEO_WITH_LUAJIT` (default `OFF`).
- When `ON`:
  - Resolves LuaJIT headers via `LUAJIT_INCLUDE_DIR` or `find_path(lua.h)` under `LUAJIT_ROOT`/`$ENV{LUAJIT_ROOT}/include`.
  - Resolves library via `LUAJIT_LIBRARY` or `find_library` for `luajit` under `LUAJIT_ROOT`/`$ENV{LUAJIT_ROOT}/lib`.
  - If missing: `FATAL_ERROR`.
  - Adds `src/runtime/lua_jit_host.cpp` to `arizona_neo_core`.
  - Adds include dir and links `${LUAJIT_LIBRARY}` publicly to `arizona_neo_core`.
  - Defines `ARIZONA_NEO_WITH_LUAJIT=1`.
- Without the option, Core builds normally (no LuaJIT dependency).

Example (Linux host, external SDK):
```bash
cmake -S . -B build-luajit -DARIZONA_NEO_WITH_LUAJIT=ON \
  -DLUAJIT_INCLUDE_DIR=/path/to/luajit/include \
  -DLUAJIT_LIBRARY=/path/to/luajit/lib/libluajit.a
cmake --build build-luajit
ctest --test-dir build-luajit --output-on-failure
```

## 7. Lua state lifecycle
1. `initialize()`: if already initialized returns `true`; calls `luaL_newstate()`, opens minimal libs, sets `initialized_=true`. On failure sets `last_error_`, closes state if created, returns `false`.
2. `execute(source, chunk_name)`: requires initialized state; loads buffer, pcalls, resets stack to empty; returns `true` on success.
3. `shutdown()`: closes state (if any), resets `initialized_=false`, safe to call multiple times / after failed init.

## 8. JIT / FFI policy (обновлено в Stage 6.2.4)
- JIT disabled: `XCFLAGS="-DLUAJIT_DISABLE_JIT"` → `LJ_HASJIT=0`. Воспроизводимая linux-host сборка: `scripts/build_luajit_linux_host.sh`. Поверено поведенчески через интегрированный `LuaJITHost`: `jit.status() == false` и `select('#', jit.status()) == 1`; повторная сборка дала байт-идентичный архив (sha256 `963c7ea3…`). Ранее ошибочное утверждение Stage 6.2.3 про «JIT включён» исправлено.
- FFI включён (`luaopen_ffi` присутствует, `LUAJIT_DISABLE_FFI` не определён). Тест через `LuaJITHost`: `require('ffi')`, `ffi.cdef`, `ffi.new`, struct fields, обработка FFI-ошибки без разрушения state. `ffi.load` не тестируется и не рекомендуется.
- iOS-устройство: JIT/FFI runtime не проверялся в этом этапе.

## 9. Tests (обновлено в Stage 6.2.4)
- `tests/lua_runtime_test.cpp`. Without `ARIZONA_NEO_WITH_LUAJIT` тест выдаёт SKIPPED и завершается успешно; проверены и Debug, и Release без LuaJIT.
- Покрыто: initialize/is_initialized/повторный initialize/shutdown/повторный shutdown/re-init; execute (2+2, assert-скрипты, последовательные chunk); syntax error, runtime error, восстановление после ошибок; globals (число/строка/отсутствует/неверный тип/nullptr); защита от `__index`-метамethod longjmp; JIT disabled через `require('jit')` + `jit.status()`/`select('#', ...)`; FFI (модуль, struct fields, FFI-ошибка и восстановление); **реальная проверка `stack_top()==0` после каждого execute/getter/error**.
- Результаты: Debug+LuaJIT и Release+LuaJIT — сборки OK, ctest 6/6; без LuaJIT Debug/Release — 6/6. ASan/LSan недоступны в PRoot (VA-лимит), см. `docs/STAGE_6_2_4_RUNTIME_VALIDATION.md`.

## 10. Platform notes
- **Linux host:** use a LuaJIT 2.1 built from the pinned commit (`96f7b29…`) with matching headers/libs; do not mix iOS/macOS binaries.
- **iOS (arm64):** use the archived `libluajit.a` from run `45253149431` via `LUAJIT_INCLUDE_DIR`/`LUAJIT_LIBRARY` (or `LUAJIT_ROOT`). Compile/link validation is the expected CI check for integration; device runtime not claimed.

## 11. References
- `docs/LUAJIT_IOS_BUILD.md`
- `docs/LUA_RUNTIME_RESEARCH.md`
- `docs/PORTING_PLAN.md`

## 12. Note (Stage 6.2.1 audit, дополнено в Stage 6.2.4)
- LuaJIT 2.1 follows Lua 5.1 API: `luaL_requiref` is not available; library opening uses protected `lua_pcall` with `luaopen_*` + `lua_setglobal` pattern (or equivalent). The implementation was adjusted accordingly.
- `lua_close` invokes Lua `__gc` finalizers. LuaJIT internally protects each finalizer with `lj_vm_pcall` (lj_gc.c:522), but arbitrary Lua code can run there; C++ exceptions do not catch Lua longjmp, so `shutdown()` must be called with no live C++ objects requiring unwinding and finalizer side effects must not be relied upon.
- `lua_getglobal` may invoke `__index` metamethods; global getters now use a protected C trampoline under `lua_pcall` (Stage 6.2.4).
- Полный отчёт: `docs/STAGE_6_2_4_RUNTIME_VALIDATION.md`.

## 13. Stage 6.2.5 — iOS artifact validation (обновление)
- iOS `libluajit.a` из run 45253149431 (artifact 14855513382) загружен и проверен: 70 объектов Mach-O ARM64, platform IOS, minos 15.0.0, JIT disabled (`LJ_OS_NOJIT` + `XCFLAGS="-DLUAJIT_DISABLE_JIT"`), FFI enabled (`luaopen_ffi`), все Lua C API-символы определены, ELF/Bionic/JNI-зависимостей нет.
- Локальный хост — Linux (нет Xcode), поэтому iOS compile/link Core+LuaJIT и runtime на устройстве в Stage 6.2.5 **не выполнялись**. Подготовлен `scripts/validate_ios_luajit_core.sh` для macOS-15 runner; существующий `ios-core.yml` пока собирает Core без LuaJIT.
- Подробности и blockers: `docs/STAGE_6_2_5_IOS_INTEGRATION_VALIDATION.md`.
