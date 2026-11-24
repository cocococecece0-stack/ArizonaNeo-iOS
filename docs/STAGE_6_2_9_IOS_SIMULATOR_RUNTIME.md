# Stage 6.2.9 — iOS Simulator Runtime Validation (подготовлено, НЕ ВЫПОЛНЕНО)

Статус: **подготовка завершена, но критерий PASS не достигнут**. По явному указанию в этом этапе не выполнялись commit/push, поэтому новый workflow `.github/workflows/ios-simulator-runtime.yml` отсутствует на remote и не может быть запущен через GitHub Actions. Фактического выполнения LuaJITHost/FFI внутри iOS Simulator нет.

## 1. Аудит
- `src/runtime/lua_jit_host.cpp` содержит `extern "C"`-обёртку Lua-заголовков (Stage 6.2.3), защищённые геттеры globals и `stack_top()` (Stage 6.2.4).
- `CMakeLists.txt`: `ARIZONA_NEO_WITH_LUAJIT` (OFF по умолчанию), `LUAJIT_INCLUDE_DIR`/`LUAJIT_LIBRARY`, линкует `m`.
- `tests/CMakeLists.txt`: прежние 6 тестов + `lua_runtime_test` только при `ARIZONA_NEO_WITH_LUAJIT=ON`.
- Stage 6.2.8 (run 45349371946, attempt 2) подтвердил iOS device ARM64 compile+link с реальным LuaJIT artifact, но **не выполнял код на устройстве**. Device-библиотека имеет `LC_BUILD_VERSION platform 2 (IOS)`, minos 15.0 — для Simulator она не используется.
- `scripts/build_luajit_ios.sh` использует `-miphoneos-version-min=15.0` (device) — оставлен без изменений.
- LuaJIT commit везде закреплён: `96f7b297326e9fd454b7186c6858b2773f94a0dc`.

## 2. Отдельная сборка LuaJIT для Simulator
Добавлен `scripts/build_luajit_ios_simulator.sh` (только macOS/Xcode):
- `TARGET_SYS=iOS`, `TARGET_LJARCH=arm64`, `BUILDMODE=static`, `XCFLAGS="-DLUAJIT_DISABLE_JIT"`;
- `TARGET_CC="clang -arch arm64 -isysroot <iphonesimulator SDK> -mios-simulator-version-min=15.0"` — platform выбирается SDK/flag, **без правки Mach-O metadata**;
- жёсткая проверка commit, `lipo -info` (arm64), `otool -hv` (ARM64), `LC_BUILD_VERSION platform 7 (IOSSIMULATOR)` и `minos 15.0`, `luaL_newstate`/`lua_pcall`/`lua_close`/`luaopen_ffi`/version symbol, отсутствие JIT-символов, ELF/Bionic/JNI;
- output: `.luajit-ios-simulator-build/out-ios-simulator-arm64/libluajit.a`.

LuaJIT Makefile при `TARGET_SYS=iOS` задаёт `LUAJIT_TARGET_IPHONE=1` → `LJ_OS_NOJIT=1` (JIT off), `LJ_HASFFI=1` (FFI on) — конфигурация поддерживается официальным механизмом.

## 3. Test harness
Добавлены:
- `tests/ios_simulator/SimRuntimeTests.mm` (XCTest suite, выполняется в симуляторе)
- `tests/ios_simulator/SimRuntimeTests.xcodeproj/` (project.pbxproj + scheme, bundle id `com.arizonaneo.SimRuntimeTests`)
- `tests/ios_simulator/sim_runtime_test.cpp` (Linux sanity harness, не является iOS-доказательством) — реальные(check) сценарии через `LuaJITHost`: initialize/re-init, `20+22==42`, globals, доступность base/math/string/table/package/ffi/jit (io/os/debug намеренно не открываются), FFI struct `NeoTestPoint {12,30}` → 42, `jit.status()` false + 1 значение, syntax/runtime error и восстановление, несколько execute, shutdown/повторный shutdown/re-init, `stack_top()==0`. Возвращает 0 только при всех PASS.
- `tests/ios_simulator/CMakeLists.txt` — отдельный проект, линкует уже собранные `libarizona_neo_core.a` и simulator `libluajit.a`, C++17.

Запуск в workflow: `xcrun simctl boot <UDID>` + **`xcodebuild test -project tests/ios_simulator/SimRuntimeTests.xcodeproj -scheme SimRuntimeTests -destination "platform=iOS Simulator,id=$UDID" -resultBundlePath ...`**. XCTest-бандл (bundle id `com.arizonaneo.SimRuntimeTests`) устанавливается и запускается XCTest runner **внутри процесса iOS Simulator**; результат берётся из exit code `xcodebuild` и `.xcresult`. Произвольный `simctl spawn` бинарник больше не используется — это не доказательство запуска iOS-приложения.

## 4. Workflow (подготовлен, не запущен)
`.github/workflows/ios-simulator-runtime.yml`, `workflow_dispatch`, `macos-15`, выбор доступного iPhone Simulator с iOS 18.x через `xcrun simctl list devices available -j`, boot, сборка LuaJIT simulator, сборка Core (`ARIZONA_NEO_WITH_LUAJIT=ON`), сборка XCTest-бандла, установка/запуск в симуляторе через `xcodebuild test`, проверка exit code и `.xcresult`, проверка `platform 7`, отчёт по каждому тесту, SHA256-манифест, upload-artifact `ios-simulator-luajit-runtime` (retention 30 дней). Существующие device-воркфлоу не изменялись.

## 5. Что НЕ подтверждено
- Сама сборка LuaJIT для Simulator, сборка Core, линковка и выполнение в симуляторе — **не запускались** (net Xcode на локальном Linux-хосте; workflow не на remote из-за запрета push).
- SHA256 simulator-артефактов — нет (не собраны).
- Результаты Lua/FFI/lifecycle внутри Simulator — нет.
- Подтверждение работы на физическом iPhone — по-прежнему только compile/link (Stage 6.2.8).

## 6. Блокер
Требуется разрешение на commit/push новых файлов (скрипт, harness, workflow, документы) и последующий dispatch `ios-simulator-runtime.yml`. До этого Stage 6.2.9 не считается успешным.

Локальные проверки: `git diff --check` (0), YAML parse, `bash -n` workflow run-блоков и скриптов. Два ранее оставленных локальных изменения (`docs/LUA_RUNTIME_RESEARCH.md`, дополнение к `docs/STAGE_6_2_6_GITHUB_IOS_CORE_BUILD.md`) не затрагивались.
