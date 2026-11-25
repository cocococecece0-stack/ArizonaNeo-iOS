# Stage 11.0 — Native iOS Bootstrap Implementation

## Реальная архитектура
Отдельная iOS ARM64 dynamic library `libNeoLoaderBootstrap.dylib` (>= iOS 15.0), собраная из текущего checkout и линкованная со свежесобранным `arizona_neo_core` (LuaJIT ON). Bootstrap не содержит Android JNI/ELF/Bionic зависимостей и не вызывает game API.

Состояния C ABI:
- `NOT_STARTED` → `INITIALIZING` → `BOOTSTRAP_READY_HOST_PENDING` → (после `neo_bootstrap_attach_host` с реальными function pointers) `RUNTIME_READY`;
- `FAILED` при ошибке инициализации; `STOPPED` после shutdown; повторная инициализация после shutdown разрешена.

`BOOTSTRAP_READY_HOST_PENDING` означает: библиотека загружена, portable runtime создан, LuaJIT инициализирован, но игровые capabilities остаются недоступными (`game_capabilities_available == 0`).

## Созданные файлы
- `ios/bootstrap/include/neo_bootstrap.h` — стабильный C ABI.
- `ios/bootstrap/src/bootstrap.cpp` — реальная реализация, constructor только фиксирует факт загрузки.
- `tests/ios_bootstrap_test.cpp` — 12 проверок C ABI/lifecycle/thread-safety.
- `CMakeLists.txt` — target `neo_loader_bootstrap` (SHARED) + PIC для Core.
- `.github/workflows/ios-device-runtime.yml` — сборка dylib из текущего checkout, проверка platform 2/minos 15.0, exported symbols, SHA256, публикация артефакта.

## Способ инициализации
1. `__attribute__((constructor))` только выставляет `g_library_loaded` (без тяжёлой работы).
2. `neo_bootstrap_initialize()` (mutex, idempotent): EventBus, Scheduler, ScriptManager, SandboxedFilesystem, LuaJITHost, NeoLoaderHost; при LuaJIT ON runtime доступен.
3. `neo_bootstrap_get_status()` возвращает версию bootstrap, версию Core, состояние, длительность, error, shutdown fact, runtime/game capability флаги.
4. `neo_bootstrap_attach_host()` принимает только реальные function pointers; без них возвращает `INVALID_ARGUMENT`, capability не повышается.
5. `neo_bootstrap_shutdown()` безопасно освобождает runtime, выставляет `STOPPED`, повторный вызов permissible.

## Подключение к Core
Используются существующие `LuaJITHost`, `EventBus`, `Scheduler`, `ScriptManager`, `SandboxedFilesystem`, `NeoLoaderHost` — без дублирования классов. NeoLoaderHost собирается только при `ARIZONA_NEO_WITH_LUAJIT=ON`.

## Ограничения
- Библиотека НЕ загружена внутрь Arizona RP; нужен отдельный in-process integration milestone.
- Игровые APIs не вызываются; capabilities остаются недоступны до реального `attach_host`.
- Диагностика пишется только в diagnostics struct; файл отчёта не создаётся (нет абсолютных путей Android/Termux).
- Без LuaJIT runtime_available == 0, состояние остаётся BOOTSTRAP_READY_HOST_PENDING.

## Результаты сборки
- Linux Debug + LuaJIT ON: 12/12 PASS (включая `ios_bootstrap_test`).
- Linux Release + LuaJIT ON: 12/12 PASS.
- Linux LuaJIT OFF: 9/9 PASS.
- iOS CI run `45641509524`: success. Dylib: Mach-O 64-bit arm64 shared library, platform 2, minos 15.0, sdk 18.5.
- Экспортированы: `neo_bootstrap_initialize`, `neo_bootstrap_shutdown`, `neo_bootstrap_get_status`, `neo_bootstrap_get_version`, `neo_bootstrap_attach_host`.

## SHA-256 артефакта
`c74acb30deaac7846a1e3bf0d4a2a0407fa029a8d7799355fa4eafcea71b83c9`
(путь в CI: `ci/build-core/libNeoLoaderBootstrap.dylib`; опубликован в artifact `ios-device-luajit-test-host`, run `45641509524`).

## Commit
Commits: `863bd94` (feat(ios): implement native NeoLoader bootstrap dylib), `ed4d41c`, `b647f6b`, `49ef602` (CI fixes). Head `49ef602`, branch `stage-7.1.7-device-validation`, `main` не изменён.

## Следующий необходимый шаг
Получить подтверждённый in-process entrypoint в Arizona RP, который загрузит `libNeoLoaderBootstrap.dylib` и вызовет `neo_bootstrap_attach_host()` с реальным function pointer `_game_set_graphics_failure_handler`; затем проверить загрузку внутри игры в отдельном integration milestone.
