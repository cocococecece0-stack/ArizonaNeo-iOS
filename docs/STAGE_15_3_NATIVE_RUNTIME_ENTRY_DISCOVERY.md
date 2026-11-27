# Stage 15.3 — Native Runtime Entry Discovery

Дата: 2026-11-26.

## 1. Preflight

- Ветка: `stage-7.1.7-device-validation`.
- HEAD: `d7f33bc16e9d90bdffa1927fa68486d77b266c41`.
- `main` не изменялся.
- `git reset`, `clean`, `rebase`, force push не выполнялись.
- Commit/push не выполнялись.
- Все изменения Stage 15.2/15.2R сохранены.
- Pre-existing изменения `docs/LUA_RUNTIME_RESEARCH.md` и
  `docs/STAGE_6_2_6_GITHUB_IOS_CORE_BUILD.md` не трогались.
- `.research-inputs/` и `research-inputs/` не изменялись и не добавлялись в Git.

## 2. Артефакты и метод

Исследован интегрированный IPA:

`/root/ArizonaNeo-research/stage-13.4/ArizonaNeo-integrated-unsigned.ipa`

- IPA SHA-256: `de39850c79ee6f0658e01e6e01affb8aadb512dfe7ad86844a907141c56f42cb`.
- Основной executable: `Payload/Arizona.app/Arizona`,
  15 050 336 bytes,
  SHA-256 `c49213bec73b97379b3566cb0316227b926b93089e814c290c93a4d69073888a`.
- `Frameworks/libag-client.dylib`: 11 411 728 bytes,
  SHA-256 `0d097d5088ed232295cc8f7d10cd32dd5143fa80ef6cf9a7c85ba2cb92e1763b`.
- `Frameworks/libNeoLoaderBootstrap.dylib`: 631 792 bytes.

Бинарники только читались. Игра не запускалась. Для анализа использовались:

- `llvm-objdump --macho --syms`;
- `llvm-objdump --macho --disassemble --dis-symname ...`;
- `llvm-objdump --macho --objc-meta-data`;
- `llvm-objdump --macho --dyld-info`;
- `llvm-objdump --macho --exports-trie`;
- разбор Mach-O `__objc_classlist`, `__objc_const`, `__objc_selrefs`
  и ELF-заголовка `libag-client` на Python;
- `strings`, `readelf` для извлечённого ELF image.

## 3. Frame/tick кандидаты

| Кандидат | Адрес | ABI | Caller | Вывод |
|---|---:|---|---|---|
| `_ui_frame_tick` | `0x100925f70` | `void(void)` | `_bionic_eglSwapBuffers` `0x1009223bc`, вызов `0x1009224d0` | **Подтверждённая per-frame точка** перед `presentRenderbuffer`; не callback registry |
| `GLView.tick` | IMP `0x10001b0bc` | `void(id self, SEL _cmd)` | `CADisplayLink` target=self, selector=`tick` | Static callback path подтверждён; тело `tick` — `ret`; runtime firing не доказан |
| `_ui_poll_set_clock` | `0x100925ee4` | `void(uint64_t (*clock)(void))` | прямых вызовов нет; вызывается извне/через UI | Меняет clock-функцию `0x100ddc070`, не frame callback |
| `_ui_set_handler` | `0x100925bf8` | `void(void* context, void (*handler)(...))` | Swift caller `0x100875ca0` | Java/UI bridge handler, не frame |
| `_game_set_graphics_failure_handler` | `0x100924e88` | `void(void* ctx, void (*handler)(void*))` | Stage 8.1 caller не найден | Подтверждённый graphics-failure callback, не frame |
| `_game_touch` | `0x100924eb8` | аргументы зависят от touch-события | `GLView.touchesBegan` `0x10001b530` | Input callback, не frame |
| `_game_key` | `0x100924fd4` | keyboard path | `0x10001d680` | Input callback, не frame |

### 3.1 `_ui_frame_tick`

Ключевое доказательство:

```text
_bionic_eglSwapBuffers 0x1009223bc
  0x10092247c: glGetIntegerv(GL_FRAMEBUFFER_BINDING, &current)
  0x1009224c4: ldr w9, [x19, #0x18]       ; default framebuffer
  0x1009224c8: cmp w8, w9
  0x1009224cc: b.ne 0x1009224dc
  0x1009224d0: bl _ui_frame_tick
  0x1009224d4: ... glInvalidateFramebuffer
  0x100922518: bl 0x10092d660             ; presentRenderbuffer:
```

Вывод:

- `_ui_frame_tick` вызывается из `_bionic_eglSwapBuffers` при bound default
  framebuffer, непосредственно перед `presentRenderbuffer`.
- Это **реальный per-frame путь** для OpenGL ES, а не просто наличие функции.
- ABI: без аргументов и без возвращаемого значения.
- Внутри `_ui_frame_tick`:
  - `pthread_mutex_trylock(0x100ddc278)`;
  - при занятом mutex — немедленный выход;
  - если `[0x100e2d688] != 0`, вызов clock-функции из `[0x100ddc070]`;
  - проверка 200 ms и `_veh_mem_poll(0)`.
- `_ui_poll_set_clock` записывает clock-функцию в `0x100ddc070`; при `x0 == 0`
  используется `0x100925f08` (`mach_absolute_time`/timebase).
- В `libag-client` есть строка `eglSwapBuffers`, а main экспортирует
  `_bionic_eglSwapBuffers`; однако точная runtime-привязка import → shim
  (через кастомный ELF loader) не подтверждена. Это остаётся частью
  device/runtime проверки.
- Это **не** render callback registration. Это стабильная функция, которую
  вызывает renderer; для подключения нужен hook/interpose, а не вызов
  `NeoLoaderHost` из неё напрямую.

### 3.2 `GLView.tick` / CADisplayLink

Класс `_TtC7Arizona6GLView` найден в `__objc_classlist`; его method list
содержит:

```text
tick  IMP 0x10001b0bc  types v16@0:8
```

`startAnimating` `0x10001afac`:

```text
selref tick = 0x100d92378
[CADisplayLink displayLinkWithTarget:self selector:@selector(tick)]
addToRunLoop:forMode:NSRunLoopCommonModes
set frame rate range (CAFrameRateRange)
```

`startAnimating` вызывается из `0x100006970` и `0x100008ad8`.
`0x100006cb4` (surface/resume path) вызывает `0x100006868`, который
вызывает `_game_set_frame_limit` и затем `startAnimating`.

`tick` IMP `0x10001b0bc` — это `ret` (пустой метод). Значит:

- механизм CADisplayLink → `objc_msgSend(GLView, @selector(tick))` статически
  подтверждён;
- original callback ничего не делает;
- без runtime trace нельзя доказать, что display link активен именно в
  игровом процессе и что его частота совпадает с render loop;
- безопасный способ подключения — ObjC method swizzling `tick`, но это
  production-изменение, которое нельзя считать проверенным без запуска.

### 3.3 `_ui_set_handler` / `_ui_java_call`

`_ui_set_handler` `0x100925bf8` вызывается из Swift-кода `0x100875ca0`:

```text
x0 = 0x100878674
x1 = 0
```

Setter сохраняет:

- `x1` → `0x100e2d670`;
- `x0` → `0x100e2d678`.

`_ui_java_call` `0x100925fe8`:

- `_ui_parse_args` `0x100925c0c` возвращает число аргументов в `x0`;
- handler вызывается как:
  `handler(method_string, parsed_args, argc, userdata)`.

Это Java/UI bridge, а не frame callback. Использовать его как frame/tick
нельзя.

## 4. Input и lifecycle

Подтверждённые callers:

- `_game_touch` `0x100924eb8` вызывается из
  `GLView.touchesBegan:withEvent:` (`0x10001b530`).
- `_game_key` `0x100924fd4` вызывается из `0x10001d680`.
- `_game_surface_created` `0x100924da0` вызывает
  `Java_com_arizona_game_natives_SurfaceCreated`.
- `_game_resize` `0x100924b60` вызывает
  `Java_com_arizona_game_natives_Resize`.
- `_game_resume` `0x100924aa8` вызывает
  `Java_com_arizona_game_natives_Resume`.
- `_game_pause` `0x100924c30` вызывает
  `Java_com_arizona_game_natives_Pause`.

Это lifecycle/input hooks, но не SAMP event source и не per-frame Lua
dispatch.

## 5. SAMP event source

Проверено:

- original `lib/samp/events.lua` требует `samp.raknet`, `samp.events.core`,
  `RakLua` и `RakLuaSO`;
- `RakLuaSO` отсутствует в распаковке NeoLoader и в iOS-бинарнике;
- native RakNet callbacks (`onReceiveRpc`, `onSendRpc`, `onReceivePacket`,
  `onSendPacket`) не подтверждены;
- в `libag-client.dylib` есть stripped ELF и C++ RTTI-имена
  `libPED::RPC::*`, но нет экспортированного Lua/C callback для
  `onServerMessage` или SAMP RPC;
- строка `Java_com_arizona_game_GTASA_onDialogResponse` и `onDialogResponse`
  относятся к JNI UI → native, а не к server → Lua event;
- `showPlayerDialog`, `setDialogInfo`, `doDialogResponse` в `libag-client`
  являются UI/web bridge строками, не подтверждённым event dispatcher.

Итог: **SAMP event source BLOCKED**. Нельзя вызывать
`samp.events.dispatch` из игры без подтверждённого native hook.

## 6. Production entrypoint

Что уже есть:

- `libNeoLoaderBootstrap.dylib` загружается через `LC_LOAD_DYLIB`;
- constructor `neo_bootstrap_library_loaded` пишет `DYLIB_LOADED`;
- exports: `neo_bootstrap_initialize`, `neo_bootstrap_attach_host`,
  `neo_bootstrap_shutdown`, status/get_marker_path;
- `_game_set_graphics_failure_handler` экспортирован (`0x100924e88`) и может
  быть получен через `dlsym`;
- `_game_resume`/`_game_surface_created` — подтверждённые lifecycle callers.

Чего нет:

- production caller `neo_bootstrap_initialize()`;
- production caller `neo_bootstrap_attach_host()`;
- безопасного, уже проверенного способа вызывать `NeoLoaderHost::start()`;
- runtime trace, доказывающего, что выбранный frame hook срабатывает в
  игровом процессе.

`GLView.tick` swizzle — наиболее перспективный безопасный кандидат:
ObjC runtime, без патча машинного кода. Но original `tick` пустой, а его
фактическое срабатывание не подтверждено на устройстве. `_ui_frame_tick`
подтверждён как per-frame вызов, но это прямой `bl` внутри main executable;
его hook требует patch/interpose, что нельзя считать безопасным без
отдельной проверки.

## 7. Реализация Stage 15.3

**Production-код не изменялся.** Это осознанное решение по критерию B:

- подтверждён per-frame caller `_ui_frame_tick` и CADisplayLink-кандидат
  `GLView.tick`;
- но нет runtime-доказательства срабатывания и нет безопасного, уже
  проверенного способа подключения без code-patching;
- SAMP event source не подтверждён;
- поэтому нельзя честно объявить ни `RUNTIME_READY`, ни SAMP events.

Существующие Stage 15.2/15.2R изменения сохранены. Никакие неподтверждённые
адреса/ABI не добавлены в production-код.

## 8. Тесты

Запущены последовательно, без параллельного ctest с общими temp paths:

- LuaJIT ON, Debug: **16/16 passed**;
- Lua 5.4 ON, LuaJIT OFF: **12/12 passed**;
- LuaJIT OFF, Lua 5.4 OFF: **11/11 passed**;
- `git diff --check`: без ошибок.

iOS CI/сборка/симулятор/физический iPhone не запускались: в окружении нет
macOS/Xcode. Это не iOS device validation.

## 9. Следующий проверяемый шаг

1. Собрать диагностическую iOS-сборку с **read-only frame probe**:
   - swizzle `GLView.tick` через ObjC runtime;
   - записывать `FRAME_TICK` marker не чаще 1 раза в секунду;
   - не инициализировать Lua в render callback;
   - проверить на устройстве через ESign/Console/marker, что callback
     действительно срабатывает.
2. Если probe подтверждён — добавить безопасную очередь кадров и
   `NeoLoaderAPI::tick(delta)` на отдельном worker/main-loop drain; только
   после этого вызывать `neo_bootstrap_initialize()` и
   `neo_bootstrap_attach_host()`.
3. Для SAMP: получить unstripped `libag-client` или runtime trace RPC
   handlers; подтвердить symbol/ABI `onReceiveRpc`/`onSendRpc` или
   эквивалентный C++ hook. Без этого `samp.events.dispatch` остаётся
   BLOCKED.
4. Проверить constructor/`DYLIB_LOADED` и `GLView.tick` на реальном iPhone;
   только после этого обновлять статус production entrypoint.

## 10. Вердикт

Stage 15.3 завершён по варианту **B**:

- найден и доказан per-frame caller `_ui_frame_tick` из
  `_bionic_eglSwapBuffers` (внутри main executable; runtime-привязка
  `libag-client` import → shim ещё требует проверки);
- найден статический CADisplayLink callback `GLView.tick`;
- подтверждены input/lifecycle callers;
- SAMP event source не подтверждён;
- production entrypoint не реализован, потому что для него не хватает
  runtime-доказательства и безопасного способа подключения;
- Linux-тесты не сломаны, Stage 15.2R сохранён.

## Stage 15.3.1 follow-up

Реализован диагностический `GLView.tick` probe без запуска Lua/NeoLoaderHost:
`ios/bootstrap/src/ios_frame_probe.mm`, `neo_ios_frame_probe_*` C ABI,
`neo_bootstrap_write_marker` и portable `FrameProbeState`.
Подробности и инструкция device-проверки:
`docs/STAGE_15_3_1_DEVICE_FRAME_PROBE.md`.
