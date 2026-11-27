# Stage 15.2 — Real User Scripts Compatibility

Дата: 2026-11-26.

## 1. Preflight

- Ветка: `stage-7.1.7-device-validation`.
- HEAD: `d7f33bc16e9d90bdffa1927fa68486d77b266c41`.
- `main` не переключался и не изменялся.
- `git reset`, `clean`, `rebase`, force push не выполнялись.
- Commit/push не выполнялись.
- Существующие пользовательские изменения `docs/LUA_RUNTIME_RESEARCH.md` и
  `docs/STAGE_6_2_6_GITHUB_IOS_CORE_BUILD.md` сохранены без изменений.
- Каталоги `.research-inputs/` и `research-inputs/` не перемещались,
  не удалялись и не добавлялись в Git.

## 2. Источник исходников

Указанный в задании архив:

`/root/ArizonaNeo-research/incoming/neomloader.zip`

по этому пути отсутствует; каталога `incoming/` нет. Поэтому проверить его
SHA-256 (`70bde6763dd6be7ac9579d6da51ff3fb8eb23aec467752ef4af9136d0eb893c9`)
невозможно. Анализ выполнен по уже распакованным исходникам, которые лежат в
`/root/ArizonaNeo-research/` и использовались в предыдущих этапах проекта.
Целостность архива не подтверждена; это ограничение зафиксировано в отчёте.

> Stage 15.2R: реальная распаковка NeoMLoader затем была найдена в Termux-home
> копии репозитория (300 файлов, 210 Lua). Fixtures проверены `cmp` и совпадают
> с оригиналами. Подробности — в разделе 14.

Исходники не исполнялись на хосте, кроме двух специально выбранных скриптов
`arzmod.lua` и `Helper_for_earning_money.lua` в тестовом sandbox. Обфусцированный
`Skip_Notify.lua` не исполнялся. Сетевые запросы скриптов не выполнялись.

SHA-256 изученных файлов:

| Файл | Размер | SHA-256 |
|---|---:|---|
| `arzmod.lua` | 1 493 | `0db5b0ad7d560e920cc6d36b00ad9f649411d1b863a932bb5cc18900f38acbe0` |
| `Helper_for_earning_money.lua` | 3 956 | `00b8e69fb5e3ec9de98bfebffd126795a76a826414aa20c6a5bd138be3a6b12c` |
| `fix_Alt_button.lua` | 13 368 | `a40ecac59fdf86355654386108312af5655824868ecce097ee7a8db60e543c03` |
| `Fishhelper.lua` | 36 652 | `1028668cf1ee5f4fd102a838716c4749cbd31d4f7926533cdd2ee24b4bbf2fe5` |
| `Market_Helper.lua` | 1 700 956 | `bbf58da43560a95be88ac0a9e4f7413a808fdd9e31be67673216a969b62ec828` |
| `AUTOPILOT_v2_9_II_voditel_dlya_Arizona_RP.lua` | 5 048 | `53fda67bb9d558df8285096d7c071db695007e87080161cb1fc2eb79ebeef849` |
| `Skip_Notify.lua` | 142 394 | `aec4c575ad7d087a92fd2d67654b2e2616cc292d37ce8513af7aa4d06f087f51` |

## 3. Изученные скрипты

| Скрипт | Состояние анализа | Итог |
|---|---|---|
| `arzmod.lua` | Полный исходник, 54 строки | Load-time совместим; runtime BLOCKED |
| `Helper_for_earning_money.lua` | Полный исходник, 109 строк | Load-time совместим; `main()` BLOCKED |
| `fix_Alt_button.lua` | Полный исходник, 272 строки | Не загружается без `mimgui`; runtime BLOCKED |
| `Fishhelper.lua` | Полный исходник, 848 строк, CP1251 | Не загружается без `ffi.load("GTASA")`/`mimgui`; runtime BLOCKED |
| `Market_Helper.lua` | 33 457 строк, CP1251/Extended ASCII; зависимости и ключевые цепочки | Не загружается; только отдельные pure-Lua части переносимы |
| `AUTOPILOT_v2_9_II_voditel_dlya_Arizona_RP.lua` | Полный исходник, 164 строки | Зависимость только `lib.moonloader`; `main()` BLOCKED |
| `Skip_Notify.lua` | Только структура, 177 строк | Обфусцированная string-table + `loadstring`; не исполнялся |

`AutoFlip.lua`, `WebMobile.lua`, `Chat MentionsV2.lua` полностью исключены.

## 4. Матрица зависимостей

### 4.1 Переносимые Lua-модули и API

| API / зависимость | Скрипты | Уже было в HEAD | Чего не хватало | iOS-реализуемость |
|---|---|---|---|---|
| `require` с sandbox | все | `LuaJITHost`, `Lua54Host`, `PortableScriptLoader` | Единого backend-neutral слоя не было для `NeoLoaderAPI`/`NeoLoaderHost` | Да, реализовано |
| `lib.samp.events` | `arzmod`, `Helper`, `Fishhelper`, `Market_Helper` | Не было | Реестр handler'ов, field-assignment, alias `samp.events` | Да, реализовано как registry; dispatch только после реального callback |
| `samp.events` | `Market_Helper` | Не было | Тот же реестр под вторым именем | Да, alias реализован |
| `lib.moonloader` | `Helper`, `Market_Helper`, `AUTOPILOT` | `scripts/neo/modules/moonloader.lua` (точная копия) | Ничего для load-time | Да |
| `sampfuncs` | `fix_Alt` | Не было | Константы priority/reliability/limits | Да, реализован переносимый subset |
| `encoding` | `fix_Alt`, `Fishhelper`, `Market_Helper` | `scripts/neo/modules/encoding.lua` | Ничего для базовых CP1251/UTF-8 операций | Да, но GUI/IO остаются заблокированы |
| `basexx` | `Market_Helper` | `scripts/neo/modules/basexx.lua` | Ничего | Да |
| `bit` | `Fishhelper`, `Market_Helper` | `scripts/neo/modules/bit.lua` | Ничего для LuaJIT; в Lua 5.4 используется операторный bit | Да |
| `lfs` | `Market_Helper` | `scripts/neo/modules/lfs.lua` (sandbox-адаптер) | Полного нативного LuaFileSystem нет | Частично, только sandbox-операции |
| `inicfg` | `Fishhelper` | `scripts/neo/modules/inicfg.lua` | Ничего | Да, но Fishhelper блокируется раньше |
| `ffi` | `fix_Alt`, `Fishhelper`, `Market_Helper` | Встроен в LuaJIT | Нет `GTASA`, RakNet, структур игры | FFI как язык есть; native-вызовы BLOCKED |
| `mimgui` | `fix_Alt`, `Fishhelper`, `Market_Helper` | Нет | Реальный iOS renderer/input backend | Нет без renderer; Lua-обёртка не считается реализацией |
| `widgets` | `Fishhelper` | Нет | Зависит от `mimgui` | Нет без renderer |
| `fAwesome6`, `fAwesome6_solid` | `Fishhelper`, `Market_Helper` | Нет | Данные шрифтов/иконок | Технически можно перенести данные, но без GUI бесполезно |
| `effil` | `Market_Helper` | Нет | Потоки/каналы | Нет как Android/PC-зависимость; замена планируется отдельно |
| `requests` | `Market_Helper` | Нет | HTTP-клиент | Нет без сетевого backend |
| `ltn12`, `socket.http` | `Fishhelper` | Нет | HTTP/socket | Нет без сетевого backend |
| `samp.raknet` | `fix_Alt` | Нет | RakNet bitstream API | Нет без подтверждённого RakNet/SAMP слоя |
| `samp.synchronization` | `fix_Alt` | Нет | Синхронизация SAMP | Нет без RakNet |
| `RakLua`/RakNet-функции | `Market_Helper` | Нет | Нативные bitstream-функции | Нет |
| `addEventHandler` | `Market_Helper` | Нет | Реестр + источник событий | Реестр возможен; реальные события BLOCKED |
| `script_name`/metadata | `Fishhelper`, `Market_Helper` | `NeoLoaderAPI` | Работало только под LuaJIT | Теперь работает и под Lua 5.4 |
| `thisScript` | `Fishhelper`, `Market_Helper` | Нет | Имя, версия, path, reload | Метаданные частично; reload BLOCKED |
| `getWorkingDirectory` | `Fishhelper`, `Market_Helper` | Нет | Путь sandbox/документов | Можно сделать sandbox-аналог, но `io` всё равно BLOCKED |
| `io.open`/`io.lines` | `Fishhelper`, `Market_Helper` | Нет (policy `io=false`) | Безопасный файловый слой | Не открывать целиком; нужен отдельный sandbox API |
| `os.clock`/`os.time` | `Fishhelper`, `Market_Helper` | `NeoLoaderAPI` | Работало только под LuaJIT | Теперь и под Lua 5.4 |
| `os.execute`/`os.remove`/`os.rename` | `Market_Helper` | Нет | Опасные операции | Не реализовывать wholesale; только безопасные замены |

### 4.2 SAMP/GTA/RakNet/input

| API / зависимость | Скрипты | Уже было | Чего не хватает | iOS-реализуемость |
|---|---|---|---|---|
| `isSampAvailable` | `Helper`, `Fishhelper`, `Market_Helper`, `AUTOPILOT` | Нет | Подтверждённый SAMP state | BLOCKED до callback |
| `PLAYER_PED` | `arzmod`, `fix_Alt`, `Helper`, `Fishhelper`, `Market_Helper` | Нет | Игровой handle/local player | BLOCKED |
| `sampGetPlayerIdByCharHandle` | `arzmod`, `Market_Helper` | Нет | SAMP native | BLOCKED |
| `sampGetCharHandleBySampPlayerId` | `arzmod` | Нет | SAMP native | BLOCKED |
| `sampGetPlayerNickname` | `arzmod`, `Market_Helper` | Нет | SAMP native | BLOCKED |
| `sampRegisterChatCommand` | `Helper`, `Fishhelper`, `Market_Helper`, `AUTOPILOT` | Нет | Chat hook/command registry | BLOCKED без chat callback |
| `sampSendChat` | `fix_Alt`, `Market_Helper` | Нет | SAMP send | BLOCKED |
| `sampProcessChatInput` | `Fishhelper` | Нет | SAMP input | BLOCKED |
| `sampAddChatMessage` | `Helper`, `Market_Helper`, `AUTOPILOT` | Нет | Chat output | BLOCKED |
| `sampShowDialog` | `Helper` | Нет | SAMP dialog | BLOCKED |
| `sampSendDialogResponse` | `fix_Alt`, `Fishhelper`, `Market_Helper` | Нет | SAMP dialog response | BLOCKED |
| `sampIsDialogActive`, `sampGetCurrentDialogId`, `sampGetDialogText` | `Market_Helper` | Нет | SAMP dialog state | BLOCKED |
| `sampGetCurrentPlayerName`, `sampIsLocalPlayerSpawned` | `Market_Helper` | Нет | SAMP state | BLOCKED |
| `sampSendClickTextdraw`, `sampGet3dTextInfoById` | `Market_Helper` | Нет | SAMP textdraw/3D text | BLOCKED |
| `onServerMessage` | `Helper`, `Fishhelper`, `Market_Helper` | Новый registry | Реальный chat/RPC dispatch | Registry готов; события BLOCKED |
| `onShowDialog` | `Fishhelper`, `Market_Helper` | Новый registry | Реальный dialog RPC | Registry готов; события BLOCKED |
| `onInitGame`, `onSetPlayerName` | `arzmod` | Новый registry | Реальные SAMP RPC | Registry готов; события BLOCKED |
| `onSendPlayerSync` | `Market_Helper` | Новый registry | Packet send callback | Registry готов; события BLOCKED |
| `onChatMessage`, `onSendCommand` | `Market_Helper` | Новый registry | Chat/RPC callback | Registry готов; события BLOCKED |
| `onCreate3DText`, `onShowTextDraw`, `onTextDrawHide` | `Market_Helper` | Новый registry | SAMP RPC/3D text callback | Registry готов; события BLOCKED |
| `getCharCoordinates` | `fix_Alt`, `Helper`, `Market_Helper` | Нет | GTA native | BLOCKED |
| `getScreenResolution` | `fix_Alt`, `Fishhelper` | Нет | GTA/window native | BLOCKED |
| `isCharOnFoot`, `isCharInAnyCar` | `fix_Alt` | Нет | GTA native | BLOCKED |
| `isPauseMenuActive` | `fix_Alt` | Нет | GTA native | BLOCKED |
| `getCharHealth`, `getPlayerMoney`, `getObjectModel` | `Market_Helper` | Нет | GTA native | BLOCKED |
| `setGameKeyState` | `Market_Helper` | Нет | Input/keys | BLOCKED |
| `clearCharTasks`, `taskCarDriveToCoord`, `getCarSpeed`, `isCharDead` | `AUTOPILOT` | Нет | GTA task/vehicle API | BLOCKED |
| `getTargetBlipCoordinates`, `getGroundZFor3dCoord`, `storeCarCharIsInNoSave` | `AUTOPILOT` | Нет | GTA native | BLOCKED |
| `samp_create_sync_data` + RakNet send | `fix_Alt` | Нет | SAMP sync structs, RakNet | BLOCKED |
| `MONET_DPI_SCALE`, `MONET_VERSION` | `fix_Alt`, `Fishhelper`, `Market_Helper` | Нет | Monet/Android platform constants | Не имеют смысла без renderer/SAMP |
| `CEF`, `WebView`, `Java`, JNI | `Market_Helper` | Нет | Android/PC host | Не переносится на iOS |
| `ffi.load("GTASA")`, `_Z12AND_OpenLinkPKc` | `Fishhelper`, `Market_Helper` | Нет | Android GTASA binary | Не переносится; Android ELF на iOS не добавлялся |
| `sampev`/`events` handlers | см. выше | Новый registry | Реальный dispatch | BLOCKED |
| `wait`, `lua_thread.create` | `Helper`, `Fishhelper`, `Market_Helper`, `AUTOPILOT` | `Scheduler` есть | MoonLoader-совместимые `wait`/threads | Можно сделать на Scheduler, но нужен lifecycle/tick |
| `thisScript():reload()` | `Fishhelper`, `Market_Helper` | Нет | Перезагрузка скрипта | BLOCKED до lifecycle |

### 4.3 Общие зависимости

- `lib.samp.events` — общая для `arzmod`, `Helper`, `Fishhelper`, `Market_Helper`.
- `ffi` и `mimgui` — общие для `fix_Alt`, `Fishhelper`, `Market_Helper`.
- `encoding` — общая для `fix_Alt`, `Fishhelper`, `Market_Helper`.
- `wait`/`lua_thread` — общие для `Helper`, `Fishhelper`, `Market_Helper`, `AUTOPILOT`.
- SAMP chat/dialog APIs — общие для `Helper`, `Fishhelper`, `Market_Helper`, `AUTOPILOT`.
- `getWorkingDirectory`/`io`/`thisScript` — общие для `Fishhelper`, `Market_Helper`.
- `fAwesome6` — общая для `Fishhelper`, `Market_Helper`.
- `sampfuncs`/RakNet-синхронизация — общая для `fix_Alt` и части `Market_Helper`.

## 5. Проверка существующей реализации

Уже существовало и не дублировалось:

- `LuaHost` — общий интерфейс; `LuaJITHost`, `Lua54Host` — backends.
- Sandboxed `require`, policy библиотек, module reader, `execute_detailed`,
  `get_global_*`, `stack_top`, `register_function`.
- `PortableScriptLoader` — load/execute/unload/reload/stop/state, один общий Lua state.
- `ScriptManager` + `PortableScriptRuntime` — регистрация/load/run/stop.
- `NeoLoaderAPI` — metadata, `neo_meta`, EventBus, Scheduler, sandbox fs,
  `os.clock`/`os.time`, game capability queries.
- `NeoLoaderHost` — `start`/`stop`, documents/module reader, script discovery,
  graphics-failure interface.
- `GameBridge`, `EventBus`, `Scheduler`, `UserDocumentsRoot`, `DocumentsAccess`.
- Модули: `moonloader`, `vector3d`, `matrix3x3`, `encoding`, `inicfg`, `lfs`,
  `bit`, `basexx`, `base64`, `sha256`, `md5`, `dkjson`, `jsoncfg`, `vkeys`,
  `timerwheel`, `binaryheap`.

Важное ограничение: до этого этапа `neo_loader_api.cpp` и
`neo_loader_host.cpp` компилировались только при `ARIZONA_NEO_WITH_LUAJIT`.
`NeoLoaderAPI` не был доступен в Lua 5.4. Это исправлено.

`PortableScriptLoader` отклонял скрипты больше 1 MiB, а `Market_Helper.lua`
занимает 1 700 956 байт. Лимит поднят до 4 MiB с сохранением верхней границы.

## 6. Что реально реализовано

### 6.1 `scripts/neo/modules/samp/events.lua`

Переносимый registry, а не RakNet-диспетчер:

- field-assignment: `sampev.onServerMessage = function(...) end`;
- чтение handler'а: `sampev.onServerMessage`;
- `add_handler`, `remove_handler`, `has_handler`, `get_handler`, `dispatch`, `clear`;
- `MODULEINFO`, `INTERFACE`, `EXPORTS` для совместимости с формой оригинального модуля;
- единый registry для `lib.samp.events` и `samp.events` через `package.loaded`;
- никаких RPC, bitstream, native-вызовов и синтетических игровых данных.

`dispatch` вызывает handler только когда его явно вызовет подтверждённый
адаптер или тест. Сам модуль события не генерирует.

### 6.2 `scripts/neo/modules/sampfuncs.lua`

Переносимый subset констант SAMPFUNCS: priority, reliability, game state,
bitstream types, send rates, limits. Константы также экспортируются как globals,
если имя ещё не занято. Native-функции SAMPFUNCS не реализованы.

### 6.3 Backend-neutral `NeoLoaderAPI` и `NeoLoaderHost`

- `neo_loader_api.cpp` и `neo_loader_host.cpp` вынесены из LuaJIT-only списка.
- `#if defined(ARIZONA_NEO_WITH_LUAJIT)` с этих файлов снят.
- CMake собирает их при включённом LuaJIT **или** Lua 5.4.
- Lua 5.4 тест теперь проверяет `NeoLoaderAPI`, `samp.events`, `sampfuncs`,
  `lib.moonloader` и реальные `arzmod.lua`/`Helper_for_earning_money.lua`.

`NeoLoaderHost::start()` по-прежнему требует подтверждённый
`graphics_failure_set_handler`; production entrypoint не добавлялся.

### 6.4 Размер скрипта

`PortableScriptLoader::kMaxScriptBytes` увеличен с 1 MiB до 4 MiB. Добавлен
тест, который загружает сгенерированный скрипт размером 1 700 956 байт — как
`Market_Helper.lua`, но без его зависимостей.

### 6.5 Реальные скрипты в тестах

Добавлены неизменённые fixtures:

- `tests/fixtures/stage15_2/arzmod.lua`;
- `tests/fixtures/stage15_2/Helper_for_earning_money.lua`.

Для них выполняются:

- load через `PortableScriptLoader`;
- регистрация handler'ов через `function sampev.on...`;
- `dispatch('onInitGame', ...)`, `dispatch('onSetPlayerName', ...)`;
- проверка wrapper-логики `arzmod.lua` на тестовых doubles SAMP-функций;
- проверка `cmdResetStat()`/`cmdZarabotok()` Helper с тестовыми doubles
  `sampAddChatMessage`/`sampShowDialog`;
- `main()` не исполняется, потому что он требует native SAMP/GTA API.

## 7. Изменённые файлы

Изменены:

- `CMakeLists.txt`
- `include/arizona_neo/runtime/portable_script_loader.hpp`
- `src/runtime/neo_loader_api.cpp`
- `src/runtime/neo_loader_host.cpp`
- `tests/CMakeLists.txt`
- `tests/lua54_host_test.cpp`
- `tests/portable_script_loader_test.cpp`

Добавлены:

- `tests/user_script_compat_test.cpp`
- `scripts/neo/modules/samp/events.lua`
- `scripts/neo/modules/sampfuncs.lua`
- `tests/fixtures/stage15_2/arzmod.lua`
- `tests/fixtures/stage15_2/Helper_for_earning_money.lua`
- `tests/fixtures/stage15_2/README.md`
- `docs/STAGE_15_2_USER_SCRIPTS_COMPATIBILITY.md`

Не изменялись:

- `docs/LUA_RUNTIME_RESEARCH.md` (pre-existing dirty);
- `docs/STAGE_6_2_6_GITHUB_IOS_CORE_BUILD.md` (pre-existing dirty);
- `.research-inputs/`, `research-inputs/`;
- `main`;
- оригинальные бинарники Arizona.

## 8. Тесты и результаты

Сборка выполнялась на Linux-хосте (Termux/aarch64), не на iOS. Для обхода
Termux-детекта CMake использовались `-DCMAKE_SYSTEM_NAME=Linux` и
`PREFIX=/data/data/com.termux/files/usr`.

### 8.1 LuaJIT ON

```text
cmake -S . -B build/stage152_luajit_debug \
  -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_BUILD_TYPE=Debug \
  -DARIZONA_NEO_WITH_LUAJIT=ON \
  -DLUAJIT_INCLUDE_DIR="$PWD/.luajit-stage621/LuaJIT/src" \
  -DLUAJIT_LIBRARY="$PWD/.luajit-stage621/LuaJIT/src/libluajit.a" \
  -DBUILD_TESTING=ON
cmake --build build/stage152_luajit_debug -j8
ctest --test-dir build/stage152_luajit_debug --output-on-failure
```

Результат: **16/16 passed**. Включая `user_script_compat_test`,
`portable_script_loader_test`, `script_manager_integration_test`,
`lua_runtime_test`, `neo_loader_diagnostics_test`.

### 8.2 Lua 5.4 ON (LuaJIT OFF)

```text
PREFIX=/data/data/com.termux/files/usr \
cmake -S . -B build/stage152_lua54_debug \
  -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_BUILD_TYPE=Debug \
  -DARIZONA_NEO_WITH_LUAJIT=OFF -DARIZONA_NEO_WITH_LUA54=ON \
  -DBUILD_TESTING=ON
cmake --build build/stage152_lua54_debug -j8
ctest --test-dir build/stage152_lua54_debug --output-on-failure
```

Результат: **12/12 passed**. `lua54_host_test` включает `NeoLoaderAPI`,
`samp.events`, `sampfuncs`, `lib.moonloader` и реальные `arzmod.lua` /
`Helper_for_earning_money.lua`.

### 8.3 LuaJIT OFF / Lua 5.4 OFF

```text
PREFIX=/data/data/com.termux/files/usr \
cmake -S . -B build/stage152_nojit_debug \
  -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_BUILD_TYPE=Debug \
  -DARIZONA_NEO_WITH_LUAJIT=OFF -DARIZONA_NEO_WITH_LUA54=OFF \
  -DBUILD_TESTING=ON
cmake --build build/stage152_nojit_debug -j8
ctest --test-dir build/stage152_nojit_debug --output-on-failure
```

Результат: **11/11 passed**. Backend-neutral источники не ломают сборку без Lua.

### 8.4 Дополнительно

- `git diff --check` — без ошибок.
- Один параллельный запуск двух `ctest` дал ложное падение `filesystem_test`
  из-за общего temp path; последовательный прогон — 16/16. Это не регрессия
  проекта.
- iOS CI workflow не запускался: в окружении нет macOS/Xcode.
- Физический iPhone/симулятор не использовались.
- Новые модули пока не добавлены в `tests/ios_device/NeoModules.h`; это
  отдельная задача для iOS device runtime.

## 9. Заблокированные функции

### `arzmod.lua`

- Load: **OK** (реальный тест).
- Runtime wrappers: BLOCKED — нужны `sampGetPlayerIdByCharHandle`,
  `sampGetCharHandleBySampPlayerId`, `sampGetPlayerNickname`, `PLAYER_PED`.
- Events: BLOCKED — нужен реальный SAMP RPC dispatch для `onInitGame` и
  `onSetPlayerName`.

### `Helper_for_earning_money.lua`

- Load: **OK** (реальный тест).
- `cmdResetStat`, `cmdZarabotok`: portable Lua-логика проверена с doubles.
- `main()`: BLOCKED — `isSampAvailable`, `wait`, `sampRegisterChatCommand`,
  `sampAddChatMessage`, `sampShowDialog`, `getCharCoordinates`, `PLAYER_PED`.
- `onServerMessage`: BLOCKED до реального chat/RPC callback.

### `fix_Alt_button.lua`

- Load: **BLOCKED** на `require 'mimgui'`.
- Runtime: BLOCKED — MImGui renderer/input, `MONET_DPI_SCALE`, GTA-функции,
  SAMP sync, RakNet bitstream, `sampfuncs` native, FFI-структуры.
- Реализовывать Lua-заглушку MImGui нельзя: это был бы фиктивный renderer.

### `Fishhelper.lua`

- Load: **BLOCKED** на `ffi.load("GTASA")` (и затем `mimgui`).
- Runtime: BLOCKED — `_Z12AND_OpenLinkPKc`, MImGui/widgets/fAwesome,
  `socket.http`/`ltn12`, `io.open`, `thisScript():reload`,
  `getWorkingDirectory(0x14)`, SAMP/GTA-вызовы, chat command.
- Возможен только перенос отдельных pure-Lua функций после выделения их из
  GUI/сетевой части; полный скрипт на iOS сейчас не загрузится.

### `Market_Helper.lua`

- Load: **BLOCKED**. Верхний уровень сразу использует
  `getWorkingDirectory`, `io.open`, `lua_thread.create`, затем `require('effil')`,
  `mimgui`, `samp.events`, `ffi`, сетевые модули.
- Размер больше не является блокером: 4 MiB лимит проходит.
- Runtime: BLOCKED — `effil`, `requests`, CEF/WebView/Java, RakNet,
  огромный MImGui UI, SAMP/GTA state, `io`/`os` операции.
- Переносимы отдельно: чистые Lua-парсеры/форматтеры, `encoding`/`basexx`/
  `bit`-части, но не как загружаемый целиком скрипт.

### `AUTOPILOT`

- Load-time зависимость только `lib.moonloader`; отдельного load-теста нет.
- `main()`: BLOCKED — SAMP availability, chat commands, GTA task/vehicle API,
  `wait`.

### `Skip_Notify.lua`

- Не исполнялся.
- Структура: string-table + декодер + `loadstring`; логика обфусцирована.
- Совместимость не оценивается без безопасного анализа/разобфускации.

## 10. Что станет возможным после подтверждённого game callback

После подтверждения native symbol + signature + ABI + caller + timing:

1. Подключить реальный SAMP RPC/packet adapter к `samp.events.dispatch`.
2. Реализовать `addEventHandler`/`removeEventHandler` с реальным источником
   событий, а не только registry.
3. Реализовать `sampRegisterChatCommand`, `sampSendChat`,
   `sampProcessChatInput`, `sampAddChatMessage`, `sampShowDialog`,
   `sampSendDialogResponse`.
4. Реализовать `wait`/`lua_thread.create` поверх существующего `Scheduler`
   с корректным script lifecycle.
5. Подключить `getCharCoordinates`, `PLAYER_PED`, `isSampAvailable` и остальные
   подтверждённые GTA/SAMP функции.
6. Для `fix_Alt`: MImGui renderer + input + RakNet sync send.
7. Для `Fishhelper`: renderer/input, sandbox `io`, HTTP backend или отключение
   updater, iOS-замена `_Z12AND_OpenLinkPKc`.
8. Для `Market_Helper`: замена `effil`, HTTP backend, альтернатива CEF/WebView,
   постепенный перенос UI и сетевых частей.

## 11. Минимальный набор недостающих native/game API

- SAMP state: `isSampAvailable`, `isSampLoaded`, `PLAYER_PED`, local player id.
- SAMP player/char mapping и nickname.
- SAMP chat: register command, send chat, process input, add chat message.
- SAMP dialogs: show, active/current id/text/title, send response.
- SAMP events: incoming/outgoing RPC/packet, `onServerMessage`, `onShowDialog`,
  `onInitGame`, `onSetPlayerName`, `onChatMessage`, `onSendCommand`,
  `onSendPlayerSync`, 3D text/textdraw events.
- GTA: coordinates, on-foot/in-car, pause menu, screen resolution, health,
  money, object model, key state; для AUTOPILOT — task/vehicle API.
- Render/input: реальный ImGui frame, font loading, key/mouse state, DPI,
  screen size. Без этого `mimgui` остаётся BLOCKED.
- RakNet: bitstream read/write/send/delete, packet priorities/reliability.
- Файлы: sandbox working directory + безопасный `io`-subset.
- Потоки: MoonLoader-совместимый `wait`/`lua_thread` на Scheduler/lifecycle.

## 12. Следующий этап

Рекомендуется **Stage 15.3 — SAMP callback bridge and chat/command lifecycle**:

1. Подтвердить в нативном коде iOS callback/entrypoint для SAMP chat/dialog/RPC
   (symbol, signature, ABI, caller, timing).
2. Подключить `samp.events.dispatch` к реальному adapter'у и добавить
   `addEventHandler` с реальным источником событий.
3. Реализовать `wait`/`lua_thread` на существующем `Scheduler` и связать с
   жизненным циклом скрипта.
4. Реализовать chat commands и минимальные SAMP dialog/chat API.
5. После этого повторить end-to-end тест `arzmod.lua` и
   `Helper_for_earning_money.lua` уже с реальными событиями, а не doubles.
6. Добавить новые модули в `tests/ios_device/NeoModules.h` и запустить
   iOS Core/device CI на macOS-хосте.

## 13. Итог

Stage 15.2 реализовал переносимую часть совместимости: реестр `samp.events`,
константы `sampfuncs`, backend-neutral `NeoLoaderAPI`/`NeoLoaderHost`, реальные
load-тесты `arzmod.lua` и `Helper_for_earning_money.lua` под LuaJIT и Lua 5.4,
а также снял искусственный лимит 1 MiB для `Market_Helper.lua`.

Полный запуск `fix_Alt_button.lua`, `Fishhelper.lua` и `Market_Helper.lua`
невозможен без подтверждённого renderer/input backend, SAMP callback'ов,
RakNet/GTASA native API и сетевого/файлового слоя. Эти функции явно обозначены
как BLOCKED; фиктивные игровые данные и фиктивный GUI не добавлялись.

---

# Stage 15.2R — Source Verification and Compatibility Completion

Дата: 2026-11-26.

## 14. Проверка реальных исходников

Реальная распаковка NeoMLoader найдена в Termux-home копии того же репозитория:

`/data/data/com.termux/files/home/ArizonaNeo-iOS/.research-inputs/neomloader-extracted/neomloader/`

Это соответствует указанному в задании пути внутри репозитория; в текущем
окружении `/root/ArizonaNeo-iOS` является отдельной рабочей копией без
`.research-inputs`, поэтому использовался фактический путь Termux-home.
Каталог `.research-inputs` не изменялся, не перемещался и не добавлялся в Git.

Проверено:

- 300 файлов;
- 210 Lua-файлов;
- все семь целевых скриптов присутствуют.

SHA-256 оригиналов:

| Файл | SHA-256 |
|---|---|
| `arzmod.lua` | `0db5b0ad7d560e920cc6d36b00ad9f649411d1b863a932bb5cc18900f38acbe0` |
| `Helper_for_earning_money.lua` | `00b8e69fb5e3ec9de98bfebffd126795a76a826414aa20c6a5bd138be3a6b12c` |
| `fix_Alt_button.lua` | `a40ecac59fdf86355654386108312af5655824868ecce097ee7a8db60e543c03` |
| `Fishhelper.lua` | `1028668cf1ee5f4fd102a838716c4749cbd31d4f7926533cdd2ee24b4bbf2fe5` |
| `Market_Helper.lua` | `bbf58da43560a95be88ac0a9e4f7413a808fdd9e31be67673216a969b62ec828` |
| `AUTOPILOT_v2_9_II_voditel_dlya_Arizona_RP.lua` | `53fda67bb9d558df8285096d7c071db695007e87080161cb1fc2eb79ebeef849` |
| `Skip_Notify.lua` | `aec4c575ad7d087a92fd2d67654b2e2616cc292d37ce8513af7aa4d06f087f51` |

## 15. Сравнение fixtures с оригиналами

`tests/fixtures/stage15_2/arzmod.lua` и
`tests/fixtures/stage15_2/Helper_for_earning_money.lua` проверены через `cmp`:
оба файла **байт-в-байт совпадают** с оригиналами из распаковки (`cmp` exit 0).
Это не упрощённые fixtures: тесты загружают реальные исходники.

Сравнение используемых библиотек:

- `lib/moonloader.lua` — точное совпадение с portable-модулем;
- `lib/encoding.lua`, `lib/basexx.lua`, `lib/base64.lua`, `lib/bit.lua`,
  `lib/lfs.lua`, `lib/sha256.lua`, `lib/md5.lua`, `lib/dkjson.lua`,
  `lib/vkeys.lua`, `lib/timerwheel.lua`, `lib/binaryheap.lua`,
  `lib/vector3d.lua`, `lib/matrix3x3.lua` — точное совпадение;
- `lib/inicfg.lua` и `lib/jsoncfg.lua` отличаются осознанно: в репозитории
  используются sandbox-версии через `neo_fs_*`; оригиналы требуют
  `getWorkingDirectory`/`io`/`script.this`;
- `lib/samp/events.lua` — оригинал требует `RakLuaSO`/RakNet и **не может
  загрузиться** на iOS (в распаковке нет `RakLuaSO`, а native RakNet callback'и
  не подтверждены);
- `lib/sampfuncs.lua` — оригинал является чистым набором констант (295 строк);
  portable `sampfuncs.lua` в репозитории — меньший fallback, если оригинала нет.

## 16. Реализовано в Stage 15.2R

### 16.1 `thisScript()` lifecycle

- `thisScript()` возвращает **стабильную таблицу** (registry ref), а не новую
  таблицу при каждом вызове. Это важно для `Market_Helper.lua`, где
  `onScriptTerminate` сравнивает `sc ~= thisScript()`.
- Поля: `name`, `author`, `version`, `description`, `version_number`,
  `properties`, `path`.
- `path` задаётся хостом через `NeoLoaderAPI::set_script_path`; пустой путь не
  подменяется вымышленным.
- `thisScript():reload()` вызывает `NeoLoaderAPI::set_reload_handler`, если он
  установлен. Без handler'а возвращает `false` и явную ошибку
  `reload unavailable: no script lifecycle callback`; успешная перезагрузка не
  имитируется.

### 16.2 `wait` и `lua_thread`

- Установлен кооперативный Lua-runtime: `wait(ms)` и `lua_thread.create(fn, ...)`.
- `wait` допустим только внутри script thread; вызов из main chunk даёт явную
  ошибку.
- `neo_tick(delta)` сначала продвигает `Scheduler`, затем виртуальные часы
  Lua-потоков.
- `lua_thread` handle имеет `status()` (`suspended`/`running`/`dead`) и
  `error()`.
- Ошибка потока сохраняется в handle и не роняет host.
- `NeoLoaderAPI::run_main()` запускает глобальную `main()` в кооперативном
  потоке; без `main` возвращает `false`.
- Это именно кооперативная модель, а не native threads/effil. Автоматический
  вызов `main()` и `neo_tick` из игрового frame loop остаётся задачей host'а.

### 16.3 Файловая совместимость

Добавлены безопасные globals:

- `doesFileExist(path)`;
- `doesDirectoryExist(path)`;
- `createDirectory(path)`.

Все операции идут через `SandboxedFilesystem`; path traversal/absolute paths
отклоняются. Полный `io`/`getWorkingDirectory` не открывался.

### 16.4 Portable `samp.events` поверх archive layout

Оригинальный `lib/samp/events.lua` из распаковки не загружается без
`RakLuaSO`/RakNet. Поэтому `NeoLoaderHost::attach_documents` отдаёт
portable-registry для `lib.samp.events` и `samp.events` **до** поиска в
Documents. Исходник registry генерируется в
`build/.../generated/portable_modules.hpp` из
`scripts/neo/modules/samp/events.lua`.

Это позволяет реальным `arzmod.lua` и `Helper_for_earning_money.lua` загружаться
даже при наличии archive-файла `lib/samp/events.lua`. Оригинальный файл не
удаляется и не перезаписывается.

### 16.5 Изменённые файлы Stage 15.2R

Изменены:

- `CMakeLists.txt` — генерация portable module header;
- `include/arizona_neo/runtime/neo_loader_api.hpp` — `thisScript`,
  `set_script_path`, `set_reload_handler`, `run_main`, fs globals;
- `src/runtime/neo_loader_api.cpp` — lifecycle, wait/lua_thread, fs globals;
- `src/runtime/neo_loader_host.cpp` — portable `samp.events` priority;
- `tests/user_script_compat_test.cpp` — lifecycle/thread/fs tests;
- `tests/lua54_host_test.cpp` — Lua 5.4 lifecycle/thread tests;
- `tests/script_manager_integration_test.cpp` — archive-layout test;
- `tests/fixtures/stage15_2/README.md` — source verification.

Добавлен:

- `src/runtime/portable_modules.hpp.in` — шаблон встроенного registry.

### 16.6 Тесты Stage 15.2R

Добавлены проверки:

- `thisScript()` metadata, стабильная identity, path, reload callback и
  explicit failure без callback;
- `wait`/`lua_thread`/`run_main`/`neo_tick` под LuaJIT и Lua 5.4;
- ошибки потока и status handle;
- `doesFileExist`/`doesDirectoryExist`/`createDirectory` в sandbox;
- archive layout: существующий `lib/samp/events.lua` не перекрывает portable
  registry; в тесте вместо 25KB RakNet-оригинала используется sentinel
  `return { archive_marker = true }`, а сам оригинал проверен статически:
  он требует `RakLuaSO`/RakNet, которых на iOS нет;
- реальные `arzmod.lua` и `Helper_for_earning_money.lua` загружаются
  через `NeoLoaderHost`/`ScriptManager`.

## 17. Результаты тестов Stage 15.2R

Последовательные прогоны, без параллельного запуска ctest с общими temp paths:

- LuaJIT ON, Debug: **16/16 passed**;
- Lua 5.4 ON, LuaJIT OFF: **12/12 passed**;
- LuaJIT OFF, Lua 5.4 OFF: **11/11 passed**;
- `git diff --check`: без ошибок.

iOS CI, симулятор и физический iPhone не запускались: в окружении нет
macOS/Xcode. Это не iOS device validation.

## 18. Что реально работает и что заблокировано

Реально работает в Linux-тестах:

- загрузка настоящих `arzmod.lua` и `Helper_for_earning_money.lua`;
- регистрация handler'ов `samp.events` и alias `lib.samp.events`/`samp.events`;
- portable `sampfuncs` constants;
- `thisScript()` metadata/identity/reload hook;
- `wait`/`lua_thread`/`run_main`/`neo_tick` как кооперативная модель;
- sandbox filesystem globals;
- portable `samp.events` priority над archive `lib/samp/events.lua`.

Заблокировано:

- реальный источник SAMP RPC/packet событий и вызов `samp.events.dispatch`;
- `addEventHandler` с реальным игровым источником (registry-only не выдаётся за
  работающие игровые события);
- SAMP chat/dialog/player natives, `PLAYER_PED`, GTA natives;
- MImGui renderer/input, `mimgui`, `widgets`, `fAwesome6`;
- RakNet bitstream/send, `samp.raknet`, `samp.synchronization`;
- `effil`, `requests`, `socket.http`, `ltn12` network path;
- `getWorkingDirectory` и полный `io`;
- автоматический вызов `main()`/`neo_tick` из production frame loop;
- `fix_Alt_button.lua`, `Fishhelper.lua`, `Market_Helper.lua` целиком.

## 19. Что осталось для запуска внутри Arizona

1. Подключить `NeoLoaderHost`/`NeoLoaderAPI` к production iOS host entrypoint.
2. Вызывать `neo_tick(delta)` из подтверждённого game/frame callback.
3. Установить `thisScript` path и reload handler через `ScriptManager`.
4. Подтвердить и подключить SAMP RPC/packet/chat/dialog callback'и.
5. Реализовать `addEventHandler` с реальным источником событий.
6. Подтвердить GTA/SAMP natives и input/render backend.
7. Добавить portable-модули в iOS device test bundle и прогнать CI/device tests.
