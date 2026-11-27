# Stage 6.2.6 — GitHub Actions iOS ARM64 Core + LuaJIT Integration Build

Статус: **НЕ ЗАВЕРШЁН**. Workflow подготовлен и проверен локально, но **не запускался**: для dispatch на GitHub Actions требуется push нового файла workflow, а push в этом этапе запрещён. Реальной iOS-компиляции и статической линковки Core+LuaJIT в этом этапе не выполнено.

## 1. Workflow run URL и ID

- Run ID: **нет** (workflow не запускался).
- Ожидаемый URL после ручного запуска: `https://github.com/cocococecece0-stack/ArizonaNeo-iOS/actions/workflows/ios-luajit-core.yml`.
- Причина: `gh workflow run ios-luajit-core.yml` → `HTTP 404: workflow ... not found on the default branch`. Локальный файл `.github/workflows/ios-luajit-core.yml` ещё не отправлен в репозиторий. `gh workflow list` показывает только `iOS Core Build` (427831131) и `LuaJIT iOS ARM64 Build` (427979959).

## 2. Runner и версии инструментов

- Подготовленный workflow: `runs-on: macos-15`, только `workflow_dispatch` (ручной запуск), `permissions: contents: read, actions: read`.
- Фактические версии с **прошлого** LuaJIT-run `45253149431` (справочно, не этого этапа): macOS 15.7.9 (24G830), runner arch x86_64 (кросс→arm64), Xcode 16.4 (16F6), iPhoneOS SDK 18.5, Apple Clang 17.0.0 (clang-1700.0.13.5).
- Локальный хост: Linux AArch64/PRoot, `xcodebuild`/`xcrun`/`otool`/`lipo` отсутствуют, clang 21.1.8 (Termux, Android target), CMake 4.5.1. iOS-линковка на нём не пыталась.

Workflow в шаге «Record Apple toolchain» фиксирует `sw_vers`, `xcodebuild -version`, SDK version/path, `clang --version`, `cmake --version`, `gh --version` в лог и `$GITHUB_STEP_SUMMARY`.

## 3. LuaJIT commit

`96f7b297326e9fd454b7186c6858b2773f94a0dc` — зафиксирован в `env` workflow; шаг «Fetch LuaJIT headers» делает checkout и проверяет `git rev-parse HEAD`, а также [`luajit.h`] содержит `LUAJIT_VERSION "LuaJIT 2.1..."`.

## 4. SHA256 входного libluajit.a

Получен **заново** из GitHub Actions artifact (run `45253149431`, artifact `luajit-ios-arm64`, id `14855513382`) через `gh api .../artifacts/14855513382/zip`:

- ZIP: `58ff22c6ebe8019ff0f11679e81df93c4a1052605daefdb9fae935d5d724f595` (совпадает с API `digest`)
- **libluajit.a: `6b3c087831456891cf572f56aa9127b9539507b32c1552582eaf87c6deb31343`**

Оба полных хеша выставлены defaults workflow_dispatch-входов; неполные хеши как ожидаемые значения не используются.

## 5. SHA256 выходного libarizona_neo_core.a

**Нет** — Core для iOS не собирался. Workflow запишет хеш в `ci/sha256-manifest.txt` и загрузит манифест в artifact при успешном запуске.

## 6. iOS target / deployment target

`arm64-apple-ios15.0`: `CMAKE_SYSTEM_NAME=iOS`, `CMAKE_OSX_SYSROOT=iphoneos`, `CMAKE_OSX_ARCHITECTURES=arm64`, `CMAKE_OSX_DEPLOYMENT_TARGET=15.0`, `CMAKE_BUILD_TYPE=Release`. C++17 (`CMAKE_CXX_STANDARD 17`, `cxx_std_17`). JIT disabled, FFI enabled, LuaJIT 2.1 / Lua 5.1 API. macOS host architecture не используется как целевая.

## 7–9. Core compilation / static link / symbol inspection — НЕ ВЫПОЛНЕНО

Подготовленные шаги workflow (не запускались):

1. **Artifact checks (fail closed)**: ZIP SHA256, lib SHA256, `file`/`lipo -info` (arm64), каждый member — Mach-O (ELF → ошибка), `otool -hv` (ARM64), `LC_BUILD_VERSION platform 2 = IOS, minos 15.0` (MACOS=1 → ошибка), символы `luaL_newstate`/`lua_pcall`/`lua_close`/`luaopen_ffi`/version, отсутствие `lj_trace_*`/`lj_record_*`/`lj_ir_*` (JIT off), `LJ_OS_NOJIT` в headers.
2. **Core build**: cmake configure + build с `ARIZONA_NEO_WITH_LUAJIT=ON`, явными `LUAJIT_INCLUDE_DIR`/`LUAJIT_LIBRARY`; проверка, что `LuaJITHost` присутствует в `libarizona_neo_core.a`; `extern "C"`-обёртка в `src/runtime/lua_jit_host.cpp` на месте (Stage 6.2.3).
3. **Static link test**: отдельный минимальный CMake-проект создаётся в CI, `main.cpp` вызывает `LuaJITHost::initialize()`, `execute("ios_link_test_value = 2 + 2")`, `get_global_number(...) == 4.0`, `shutdown()`; линкуется с `libarizona_neo_core.a` + `libluajit.a` + `m` через настоящий Apple linker. Проверка `nm -g ... | grep ' U '` — отсутствие неразрешённых `_lua*`/`_lj*` (иначе fail). Это **compile/link-only**: executable на iOS не запускается.
4. **Binary inspection**: `file`, `lipo -info`, `otool -hv`, `otool -l`, `nm` для `libluajit.a`, `libarizona_neo_core.a`, `tests/lua_runtime_test`, link-test executable; отчёт в `ci/binary-inspection-report.txt`.

Ранее (Stage 6.2.5) был проверен **только входной артефакт**: 70 Mach-O ARM64 объектов, platform IOS, minos 15.0, 0 ELF, 0 неразрешённых внутренних LuaJIT-символов, JIT off, FFI on. Это не заменяет сборку Core+LuaJIT.

## 10. Проверка Android/Linux dependencies

Для входного артефакта (Stage 6.2.5): glibc `__errno_location`, Android `__android_*`, JNI, `libdl.so` — отсутствуют; 85 внешних символов относятся к Apple libc/libm/libSystem. Для **конечных** бинарников проверка не выполнялась (бинарники не созданы); workflow содержит fail-closed scan в шаге «Inspect resulting binaries».

## 11. Runtime testing

iOS runtime **не тестировался**: нет ни устройства, ни симулятора, ни запущенного workflow. Linux runtime-тесты Stage 6.2.4 (Debug/Release, 6/6, FFI, JIT, lifecycle) остаются отдельным подтверждением и на iOS не переносятся.

## 12. Ограничения / blockers

1. **Основной blocker**: workflow не запущен — требуется отдельное разрешение на commit/push файла `.github/workflows/ios-luajit-core.yml`. Без push `gh workflow run` возвращает 404.
2. Нет доказательств iOS compile/link Core+LuaJIT, нет выходного SHA256 Core, нет бинарного inspection report для Core.
3. Нет iOS runtime smoke test.
4. Fallback-ветка workflow (`rebuild_if_artifact_unavailable=true`) использует существующий `scripts/build_luajit_ios.sh`; она не воспроизводит pinned SHA256 артефакта, поэтому для неё проверяются структурные свойства, а исходный SHA — только для закреплённого artifact.
5. Потенциально: Apple linker может потребовать дополнительных настроек для iOS-исполняемого файла с `main()` — проверится только при реальном запуске на macOS runner.

К Stage 6.3 не переходим.

## Локальные проверки, выполненные в этом этапе

- `git diff --check` → exit 0.
- YAML workflow распарсен (`yaml` Node module): `workflow_dispatch`, `macos-15`, 11 steps, permissions корректны.
- Все 9 `run:`-блоков прошли `bash -n` (с подстановкой GitHub-выражений).
- `bash -n scripts/validate_ios_luajit_core.sh`, `bash -n scripts/build_luajit_ios.sh` → OK.
- Свежий CMake configure с LuaJIT ON (Linux, control check) → exit 0.
- `gh workflow list` → новый workflow отсутствует на remote; `gh workflow run ios-luajit-core.yml` → HTTP 404 (ожидаемо).

## 15. Фактические запуски workflow (2026-11-24) — ВСЕ ТРИ УПАЛИ

| Run ID | URL | Failed step | Причина |
|---|---|---|---|
| 45344553388 | https://github.com/cocococecece0-stack/ArizonaNeo-iOS/actions/runs/45344553388 | Fetch LuaJIT headers | `ERROR: missing header luajit.h` — `luajit.h` генерируемый/gitignored, отсутствует в чистом checkout |
| 45344876764 | https://github.com/cocococecece0-stack/ArizonaNeo-iOS/actions/runs/45344876764 | Fetch LuaJIT headers | `Makefile:322: *** missing: export MACOSX_DEPLOYMENT_TARGET=XX.YY. Stop.` |
| 45345056547 | https://github.com/cocococecece0-stack/ArizonaNeo-iOS/actions/runs/45345056547 | Configure and build Core | CMake warning: `Manually-specified variables were not used: ARIZONA_NEO_WITH_LUAJIT, LUAJIT_INCLUDE_DIR, LUAJIT_LIBRARY`;Core собран **без LuaJIT**, `grep -q LuaJITHost` завершил шаг с exit 1 |

Commits: `ed86651b1546e0864e3c43ef52f79b765336f41d` (workflow + docs), `a5eeb060097145f22e0e861b00a0f8f61cdb407b` (генерация `luajit.h`), `c63d400a23f0e26bde0c5ab3ecd7cd8c7c390c74` (`MACOSX_DEPLOYMENT_TARGET`). Все три отправлены в `origin main`, force push не применялся.

В runs 2 и 3 шаг «Verify LuaJIT iOS artifact (fail closed)» прошёл: `Non-fat ... arm64`, `platform 2` (IOS), `minos 15.0`, `_luaL_newstate`/`_lua_pcall`/`_lua_close`/`_luaopen_ffi`/`_luaJIT_version_2_1_1795119671`, `ARTIFACT_OK: Mach-O arm64, platform IOS, minos 15.0, JIT off, FFI on`.

**Root cause финального отказа**: в remote `main` отсутствуют изменения Stages 6.2–6.2.4 (`CMakeLists.txt` с опцией `ARIZONA_NEO_WITH_LUAJIT`, `src/runtime/lua_jit_host.cpp`, заголовок, `tests/lua_runtime_test.cpp`) — они остаются незакоммиченными локально по прямому запрету включать их в commit Stage 6.2.6. CI-checkout не видит LuaJITHost, поэтому статическая линковка с LuaJIT не выполнялась. Для успеха требуется отдельное разрешение на коммит кода интеграции предыдущих этапов.

Log files: `/tmp/opencode/stage626-run45344553388.log`, `/tmp/opencode/stage626-run45344876764.log`, `/tmp/opencode/stage626-run45345056547.log`. Упавшие run'ы не загрузили artifacts (`no valid artifacts found to download`).
