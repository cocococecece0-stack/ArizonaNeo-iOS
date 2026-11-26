# Stage 15.1 — Native Game Callback Discovery

## Preflight
- Branch `stage-7.1.7-device-validation`; HEAD `84887f23d03f6083f3f0c8db8c5dd7be17277f89`; `main` `889f3b2…`.
- Dirty worktree: только `docs/LUA_RUNTIME_RESEARCH.md` и `docs/STAGE_6_2_6_GITHUB_IOS_CORE_BUILD.md` (пользовательские документы сохранены, не изменялись).

## Исследованные символы
- `_game_set_graphics_failure_handler` `0x100924e88` / `_game_notify_graphics_failure` `0x100924e9c` — подтверждённый callback registration mechanism (Stage 8.1). ABI: `void set_handler(void* context, void (*handler)(void* context))`; игра вызывает `handler(context)`.
- `_ui_set_handler` `0x100925bf8` — setter по machine code сохраняет `x0`/`x1` в глобалы `0x100e2d678`/`0x100e2d670`.
- `_ui_frame_tick` `0x100925f70` — проверяет глобал `0x100e2d688`, затем вызывает function pointer из `0x100ddc070`, сравнивает timestamp и вызывает `_ui_poll_set_clock` / `_ui_set_*`. Это не простой render callback registration; ABI вызова и время вызова не подтверждены.
- `_game_boot` `0x100924570` — вызывает `_loader_set_dir`, `_loader_set_hook`, `_loader_load` (5 hardcoded names), `_jni_set_call_handler`, `_jni_find_class`.
- `_loader_load` `0x100923a4c` — специализирован на 5 имён; не generic extension point.
- `_loader_set_hook` `0x100923a40` — hook `_fwd_resolve`, не userspace callback registry.

## Render callback
- Подтверждённого render callback **нет**. `_ui_frame_tick` существует, но его caller, сигнатура и frame semantics не доказаны. Объявлять его render callback запрещено.

## Input callback
- `_game_touch` `0x100924eb8`, `_game_key` `0x100924fd4`, `_game_gamepad` `0x100925170` определены, но ABI/вызывающий код не подтверждены. Использовать нельзя.

## Chat command
- Механизм регистрации игровых команд не обнаружен. `_ui_set_handler` не доказан как chat command handler. `/ms` остаётся `UNAVAILABLE`.

## Production bootstrap
- Единственный подтверждённый production caller — graphics failure handler (Stage 8.1).
- Нет подтверждённой точки, где можно вызвать `NeoLoaderHost::start()` с реальными function pointers. Constructor dylib не выполняет инициализацию.

## Результат
- Новых подтверждённых game callbacks не найдено.
- `GameBridge` остаётся в состоянии capability `Unavailable`/`PendingConfirmedInterface`.
- GUI backend остаётся `UNAVAILABLE`.
- `/ms` остаётся `UNAVAILABLE`.

## Следующий шаг
1. Получить symbols/runtime trace с реального запуска Arizona (например, dtrace/Console) или исходники.
2. Подтвердить точную сигнатуру `_ui_set_handler` и её caller.
3. После этого реализовать isolated adapter и только тогда переводить capability в `Available`.
