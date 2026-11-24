# Stage 6.2.9.5 — Device Test Host Implementation

Статус: реализация подготовлена локально; IPA **не собран**, workflow **не запускался**, на iPhone тесты **не выполнялись**.

## 1. Назначение
Диагностическое UIKit-приложение `com.arizonaneo.devicetest` (deployment target iOS 15.0, arm64-apple-ios15.0) для проверки уже интегрированного `LuaJITHost` на физическом iPhone 11 Pro. Это не Arizona RP, не GameBridge и не замена NeoMLoader; Lua runtime — только реальный LuaJIT 2.1 из pinned commit `96f7b297326e9fd454b7186c6858b2773f94a0dc`.

## 2. Файлы
- `tests/ios_device/main.mm`, `AppDelegate.h/mm` — UIKit lifecycle + программный UI + Share Sheet.
- `tests/ios_device/DeviceRuntimeTests.h/mm` — 24 теста (PASS/FAIL/BLOCKED, сообщение, длительность).
- `tests/ios_device/ReportGenerator.h/mm` — JSON-отчёт в `Documents/ArizonaNeoDeviceTest/report.json`.
- `tests/ios_device/Info.plist` — bundle id, iOS 15.0, `UIFileSharingEnabled`, `UILaunchScreen`, arm64.
- `tests/ios_device/DeviceTestHost.xcodeproj/` — project + shared scheme; `CODE_SIGNING_ALLOWED=NO`, `SDKROOT=iphoneos`, `ARCHS=arm64`; линкует `$(CORE_LIB)` и `$(LUAJIT_LIB)`.
- `scripts/package_unsigned_ipa.sh` — проверки Mach-O (ARM64, platform 2, minos ≥15.0), символов `LuaJITHost`/`luaL_newstate`, сборка `Payload/` + unsigned IPA + SHA256 manifest + `unzip -t`.
- `.github/workflows/ios-device-runtime.yml` — `workflow_dispatch`, `macos-15`, загрузка и SHA256-проверка двух pinned artifact'ов, headers из pinned commit, Xcode build, Mach-O/symbol checks, unsigned IPA, upload-artifact (retention 30 дней).

## 3. Тесты
initialization/is_initialized/повторный initialize; арифметика 20+22; number/string globals; отсутствующие globals и nullptr; base/math/string/table/package; `jit.status()` (ожидается disabled); `require('ffi')`; `typedef struct { double x; double y; } NeoTestPoint;`; создание/чтение/изменение полей; runtime error + `last_error()`; восстановление; последовательные execute; `stack_top()==0`; shutdown/повторный shutdown/execute после shutdown; re-init + execute; финальный shutdown. При отсутствии живого `lua_State` state-dependent тесты получают `BLOCKED`, а не ложный PASS.

## 4. Отчёт
JSON: app version, iOS version, device model/machine, architecture, LuaJIT commit, JIT/FFI status, deployment target, timestamp, массив тестов, overall status, runtime errors. Экспорт через `UIActivityViewController` (iPhone/iPad) и `UIFileSharingEnabled`. Персональные идентификаторы (UDID, serial, Apple ID) не собираются.

## 5. Ограничения
- Xcode/UIKit отсутствуют на локальном Linux-хосте: Objective-C++ не компилировался, IPA не собирался.
- Workflow не запускался по прямому указанию Stage 6.2.9.5.
- Совместимость unsigned IPA с ESign не проверена эмпирически.
- Real device runtime остаётся неподтверждённым; Stage 6.2.9 не PASS.
