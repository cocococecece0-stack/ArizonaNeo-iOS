# Stage 15.0 — NeoLoader Diagnostics, Built-in Test Script, /ms

## Реализовано
- NeoLoaderDiagnostics: 12 реальных проверок runtime, ScriptManager, PortableScriptLoader, NeoLoaderAPI, Documents, discovery, built-in script, GameBridge, GUI, /ms. Статусы NOT_STARTED/RUNNING/PASSED/FAILED/UNAVAILABLE/SKIPPED. Обязательные проверки не помечаются PASS без реального выполнения.
- Встроенный scripts/neo/scripts/neo_loader_menu.lua: загружается через ScriptManager, использует neo_log, neo_fs_*, neo_event_*, neo_schedule, neo_tick; предоставляет neo_menu_open/close/toggle/is_open и neo_loader_self_test.
- neo_log(message) в NeoLoaderAPI: append в logs/neoloader.log через sandbox.
- Diagnostic results доступны C++ (NeoLoaderDiagnostics::run()).

## Реальные production entrypoints
- Не найдены. В original Arizona RP Mach-O нет подтверждённого entrypoint для UI или chat command registration. _ui_set_handler и _game_set_graphics_failure_handler существуют, но безопасный вызов не доказан beyond graphics failure.
- /ms честно отображается как UNAVAILABLE.

## GUI
- Реальный render callback не найден. GUI backend остаётся UNAVAILABLE; GUI-модель и Lua API не выдаются за работающий экран.
- Никакого ImGui/MImGui host не создавалось.

## Зависимости
- Встроенный script использует только portable NeoLoader API и sandbox fs.
- Пользовательские скрипты из Documents/NeoLoader/scripts обнаруживаются через NeoLoaderHost::discover_scripts().
- Ошибки отдельных скриптов не ломают runtime; ScriptManager state transitions сохранены.

## Проверки
- Linux Debug + LuaJIT ON: 15/15 PASS.
- Linux Release + LuaJIT ON: 15/15 PASS.
- LuaJIT OFF: 11/11 PASS.
- iOS Core build run 45780328809: success.
- IPA SHA256: e88287205643e48b167f10f4c3346dc538a4bacd805bf2fe5459bb1148c7c2bb.
- Core source SHA совпадает с HEAD c10d11c.

## Проверено на реальном iPhone
- НЕ проводилось. DEVICE_LOAD_PASS/RUNTIME_READY не заявляются.

## Остаётся заблокировано
- Production entrypoint для diagnostics/GUI.
- Render callback для GUI.
- Chat command registration для /ms.
- Game data accessors.
- Физическая проверка всех diagnostic checks.

## Следующий конкретный шаг
Получить подтверждённый iOS host entrypoint (source или callback contract) и подключить NeoLoaderDiagnostics + builtin menu к нему; после этого собрать IPA с диагностическим UI и провести физическую проверку.
