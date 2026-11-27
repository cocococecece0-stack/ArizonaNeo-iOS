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

## Stage 15.3 addendum — Mach-O / ELF evidence

Исследован интегрированный IPA Stage 13.4
(`/root/ArizonaNeo-research/stage-13.4/ArizonaNeo-integrated-unsigned.ipa`,
SHA-256 `de39850c79ee6f0658e01e6e01affb8aadb512dfe7ad86844a907141c56f42cb`).
Основной executable: `Payload/Arizona.app/Arizona` (15 050 336 bytes,
SHA-256 `c49213bec73b97379b3566cb0316227b926b93089e814c290c93a4d69073888a`).
`libag-client.dylib`: 11 411 728 bytes, SHA-256
`0d097d5088ed232295cc8f7d10cd32dd5143fa80ef6cf9a7c85ba2cb92e1763b`.
Бинарники только читались; код игры не исполнялся.

### `_ui_frame_tick` caller найден

- `_ui_frame_tick` `0x100925f70` вызывается из
  `_bionic_eglSwapBuffers` `0x1009223bc` в `0x1009224d0`.
- Условие вызова: `glGetIntegerv(GL_FRAMEBUFFER_BINDING)` равен default
  framebuffer `[x19+0x18]`; сразу после вызова идёт
  `glInvalidateFramebuffer` и `presentRenderbuffer` через
  `_objc_msgSend` (`0x100922518`).
- Значит, это **реальная per-frame точка**, а не произвольная функция.
- ABI: `void _ui_frame_tick(void)`, аргументов нет.
- В `libag-client` есть строка `eglSwapBuffers`; точная runtime-привязка
  import → `_bionic_eglSwapBuffers` через ELF loader не подтверждена.
- Внутри: `pthread_mutex_trylock(0x100ddc278)`; если занят — выход; если
  `[0x100e2d688] != 0`, вызов clock-функции из `[0x100ddc070]`, проверка
  200 ms и `_veh_mem_poll(0)`.
- `_ui_poll_set_clock` `0x100925ee4` записывает clock-функцию в
  `0x100ddc070`; при `x0 == 0` используется `0x100925f08`
  (`mach_absolute_time`/timebase). Это не render callback registry.

### `_ui_set_handler` caller и ABI

- `_ui_set_handler` `0x100925bf8` вызывается из Swift-кода в
  `0x100875ca0` с `x0 = 0x100878674`, `x1 = 0`.
- Setter сохраняет `x1` в `0x100e2d670`, `x0` в `0x100e2d678`.
- `_ui_java_call` `0x100925fe8` читает оба значения и вызывает handler:
  `x0 = method string`, `x1 = parsed args`, `x2 = arg count`,
  `x3 = userdata`.
- `_ui_parse_args` `0x100925c0c` возвращает число аргументов в `x0`.
- Это callback для Java/UI bridge, не frame/render callback.

### `GLView.tick` и CADisplayLink

- В классе `_TtC7Arizona6GLView` есть ObjC method `tick`
  (method list IMP `0x10001b0bc`, body — `ret`).
- `startAnimating` `0x10001afac` создаёт `CADisplayLink` с target `self`
  и selector `tick` (selref `0x100d92378`) и добавляет его в
  `NSRunLoopCommonModes`.
- `startAnimating` вызывается из `0x100006970` и `0x100008ad8`;
  `0x100006cb4` (surface/resume path) вызывает `0x100006868`, который
  вызывает `_game_set_frame_limit` и `startAnimating`.
- Это подтверждённый static callback path, но `tick` — пустой метод.
  Фактическое срабатывание в конкретном игровом процессе требует runtime
  trace; без него нельзя объявлять его production entrypoint.

### Input/lifecycle

- `_game_touch` `0x100924eb8` вызывается из
  `GLView.touchesBegan:withEvent:` (`0x10001b530`).
- `_game_key` `0x100924fd4` вызывается из `0x10001d680`.
- `_game_surface_created` `0x100924da0`, `_game_resize` `0x100924b60`,
  `_game_resume` `0x100924aa8`, `_game_pause` `0x100924c30` вызывают
  native `Java_com_arizona_game_natives_{SurfaceCreated,Resize,Resume,Pause}`
  через `_loader_sym` и JNI env. Это lifecycle hooks, не SAMP events.

### SAMP events

- Original `lib/samp/events.lua` требует `RakLuaSO`/`RakNet`; `RakLuaSO`
  отсутствует в распаковке и в iOS-бинарнике, native RakNet callbacks не
  подтверждены.
- В `libag-client.dylib` есть stripped ELF и C++ RTTI-имена
  `libPED::RPC::*`, а также JNI
  `Java_com_arizona_game_GTASA_onDialogResponse` (UI → native), но нет
  экспортированного `onServerMessage`/RPC callback для Lua.
- Следовательно, SAMP event source остаётся BLOCKED; `samp.events.dispatch`
  не вызывается из игры.

### Production entrypoint

- Dylib constructor только фиксирует `DYLIB_LOADED`; вызова
  `neo_bootstrap_initialize()`/`neo_bootstrap_attach_host()` в игре нет.
- `_game_set_graphics_failure_handler` `0x100924e88` экспортирован и может
  быть получен через `dlsym`, но production caller всё равно отсутствует.
- `GLView.tick` swizzle — самый безопасный кандидат для frame entrypoint,
  но без runtime trace нельзя доказать, что display link активен в нужный
  момент. `_ui_frame_tick` требует code-patch/interpose, что опаснее.
