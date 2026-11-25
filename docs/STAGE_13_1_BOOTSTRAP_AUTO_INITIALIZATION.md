# Stage 13.1 — Bootstrap Auto-Initialization & Device Diagnostics

## Что реально реализовано
- Исправлена эмиссия constructor: Apple Clang 17 размещает его в `__TEXT,__init_offsets` (современный аналог `__mod_init_func`); добавлена	function pointer в этот section и проверка в CI/audit.
- Constructor `neo_bootstrap_library_loaded()` теперь:
  - выставляет atomically `library_loaded`;
  - пишет diagnostic event `DYLIB_LOADED` в sandbox marker;
  - логирует через `os_log_info` (subsystem/category `neo_bootstrap`);
  - **не** создаёт LuaJIT/Core/GameBridge и **не** вызывает `attach_host`.
- `neo_bootstrap_initialize()` и `neo_bootstrap_shutdown()` пишут события `BOOTSTRAP_INITIALIZING`, `HOST_PENDING`, `SHUTDOWN`, `FAILED` в os_log и marker.
- `neo_bootstrap_attach_host()` пишет `RUNTIME_READY` только после реального `NativeInterfaces` и успешного `NeoLoaderHost::start()`.
- Status API расширен независимыми флагами: `dylib_loaded`, `bootstrap_initializing`, `bootstrap_initialized`, `host_pending`, `runtime_ready_flag`, `failed_flag`, `stopped_flag`, `shutdown_called`, `runtime_available`, `game_capabilities_available`.
- Добавлен `neo_bootstrap_get_marker_path()` для получения пути diagnostic file.
- C ABI/состояния не подменяют друг друга; `RUNTIME_READY` недостижим без реального host.

## Безопасность constructor
- В constructor только C-вызовы (`getenv`, `snprintf`, `fopen/fwrite/fclose`, `os_log`) и atomic store; нет LuaJIT, C++ объектов с очистки, mutex, UIKit/Foundation.
- Не доказано, что вызов `neo_bootstrap_initialize()` из dyld initializer безопасен (статическая C++ инициализация, mutex, LuaJIT) — поэтому он **не** вызывается в constructor.

## Что проверено на Linux
- Debug + LuaJIT ON: 13/13 PASS.
- Release + LuaJIT ON: 13/13 PASS.
- LuaJIT OFF: 10/10 PASS.
- `ios_bootstrap_test`: dylib_loaded flag после загрузки .so, идемпотентность, повторная инициализация, shutdown, marker path, отсутствие ложного RUNTIME_READY.

## Что проверено в iOS CI
- Workflow `ios-device-runtime.yml` run `45676870549`: success.
- Dylib: arm64, platform 2, minos 15.0, `__TEXT,__init_offsets` present, exported `neo_bootstrap_library_loaded` + шесть C ABI символа.
- SHA256 dylib: `5f9c2710ad36b98e700194d77efbaa35287571741cac500ac6608305edc31073`.

## Интегрированный IPA
- Источник: `/root/ArizonaNeomios/inputs/Arizona-iOS-17.9.3-9281672.ipa` (оригинал не изменён; путь `incoming` в задании отсутствует, использован доступный тот же IPA).
- Output: `/root/ArizonaNeo-research/stage-13.1/ArizonaNeo-integrated-unsigned.ipa`.
- SHA256 IPA: `0de8c49bf32c9bcf050fc29f317ec4b1dbccffe72352d450b9ec588f198c6f7c`.
- Audit: `AUDIT_PASS` (15 checks: ZIP integrity, dylib SHA256, `__init_offsets`, exports, LC_LOAD_DYLIB, LC_RPATH, Info.plist, executable).
- `PACKAGING_PASS` и `MACHO_LINK_PASS` подтверждены; `DEVICE_LOAD_PASS` и `RUNTIME_READY` **не** заявляются.

## Что пока не проверено на iPhone
- Фактический вызов constructor внутри Arizona.
- Запись `DYLIB_LOADED` в marker/os_log на устройстве.
- Вызов `neo_bootstrap_initialize()` (нет in-process caller).
- Переход в `BOOTSTRAP_READY_HOST_PENDING` и особенно `RUNTIME_READY`.
- Отсутствие crash log после ESign install.

## Как получить диагностический файл с устройства
1. Подписать интегрированный IPA через ESign.profile, покрывающим `dev.local.arizona`.
2. Установить и запустить Arizona.
3. В ESign/File Manager приложения открыть контейнер → `Library/Caches/neo_bootstrap.marker`.
4. Ожидаемые строки после загрузки dylib: `event=DYLIB_LOADED detail=constructor version=1.0.0-stage13.1`.
5. При наличии caller'а: `BOOTSTRAP_INITIALIZING`, `HOST_PENDING`, при реальном host — `RUNTIME_READY`.
6. Отсутствие файла не доказывает отсутствие загрузки: проверьте ошибки записи/разрешения контейнера и crash log. Альтернативно снимите unified log (`os_log`) через Console/Mac или syslog-экспорт, доступный без Mac если ESign предоставляет просмотр логов.

## Блокеры до RUNTIME_READY
1. Нет in-process caller `neo_bootstrap_initialize()` внутри Arizona.
2. Нет подтверждённого function pointer `_game_set_graphics_failure_handler`, который можно передать в `neo_bootstrap_attach_host()`.
3. Нет подтверждённого diagnostic UI/экспорта из Arizona без Mac.
4. Требуется реальный device report с marker/os_log.

## Git
Branch `stage-7.1.7-device-validation`; `main` не изменён. Commit: `a9af8f7` и последующие stage 13.1 fixes (cmaconstructor/section/audit); HEAD будет обновлён после финального коммита отчёта.

## Вердикт
**READY FOR DEVICE DIAGNOSTICS** по части DYLIB_LOADED (есть constructor + marker + os_log и unsigned IPA, подготовленный к подписи для ESign). **READY WITH CONDITIONS** для BOOTSTRAP_INITIALIZED/RUNTIME_READY: нужны in-process caller и реальный native interface. PACKAGING_PASS не равен DEVICE_LOAD_PASS.
