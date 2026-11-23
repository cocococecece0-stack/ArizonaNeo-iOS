# NeoLoader API Inventory (Stage 4)

Источник анализа: установленная Android-версия NeoMLoader, версия `1.0.72`, build `172`, путь `/mnt/sdcard/Android/media/com.arizona21.game.web/neomloader/` (скопирован в `/root/ArizonaNeo-research` для статического анализа, в Git не добавлялся). Материалы не запускались; все выводы основаны на статическом чтении Lua-файлов и ELF-метаданных (без выполнения бинарников).

Дата повторного аудита: 2026-11-23.

## 1. Общая структура NeoLoader

- Точка входа/менеджер скриптов: `scriptmgr.lua` (77 716 байт, 2120 строк) — присутствует и в корне, и в `lib/scriptmgr.lua` (байт-в-байт).
- Версия: `version.json` → `{"version":"1.0.72","build_number":172}`.
- Профиль совместимости: `compat/profile.json` — offsets/паттерны для `libGTASA.so` и `libarzmod.so`, имя профиля `ARZMOD_GAME x64 | Version: 1738`.
- Пользовательские скрипты: корневые `.lua` (`arzmod.lua`, `AutoFlip.lua`, `WebMobile.lua`, `Fishhelper.lua`, `Market_Helper.lua`, `Skip_Notify.lua`, `fix_Alt_button.lua`, `Helper_for_earning_money.lua`, `AUTOPILOT_...lua`, `Chat MentionsV2.lua` (0 байт)).
- Библиотеки: `lib/` (52 записи верхнего уровня: 38 `.lua`-файлов + 14 каталогов; всего 207 файлов, из них 199 `.lua` и 3 `.so`).
- Конфиги: `config/` и `config/mimgui/`.
- Логи/кэш: `logs/`, `MH_chatlogs/`, `FH_daily_prices/`, `MarketHelper/`, `ST-Music/`, `resource/`.

Подсчёт файлов (воспроизводимый):
- Всего файлов в `neomloader/`: **300**.
- `.lua`: **210** (включая корневые скрипты).
- `.so`: **3** (все в `lib/MoonMonet/`).
- `.json`: 46, `.ini`: 12, `.zip`: 6, `.log`/`.bak`: 14, `.txt`: 3, `.jar`: 2, `.ttf`: 1, `.mp3`: 2.

Манифест `lib/manifest.json`: version `1.0.2`, `total_files: 122` (учитывает подмножество файлов `lib/`, не весь каталог).

## 2. Подтверждённые модули и их API

Ниже перечислены модули, чьи экспорты подтверждены чтением исходников. Пути указаны относительно `neomloader/`.

### 2.1. `lib/moonloader.lua`

Подтверждённые экспорты (таблица `return { ... }`):
- `message_prefix = TAG` — таблица уровней логов (`TYPE_INFO`, `TYPE_DEBUG`, `TYPE_ERROR`, `TYPE_WARN`, `TYPE_SYSTEM`, `TYPE_FATAL`, `TYPE_EXCEPTION`).
- `download_status` — статусы загрузки (60 констант: `STATUS_FINDINGRESOURCE` … `STATUSEX_DATAAVAILABLE`).
- `font_flag` — флаги шрифтов (`NONE`, `BOLD`, `ITALICS`, `BORDER`, `SHADOW`, `UNDERLINE`, `STRIKEOUT`).
- `d3d_prim_type` — примитивы D3D (`POINTLIST` … `TRIANGLEFAN`).
- `audiostream_state`, `audiostream_status` — состояния аудиопотока.

Побочные эффекты при `require`:
- Если в `_G` нет `Vector3D`, пытается `require('vector3d')`.
- Если нет `matrix3x3`, пытается `require('matrix3x3')`.

Зависимости: `vector3d`, `matrix3x3` (опционально).
Android API: не использует напрямую.
FFI: нет.
Нативные библиотеки: нет.
Переносимость: высокая (чистый Lua).

### 2.2. `lib/monetloader.lua`

Подтверждённые экспорты:
- `touch_events` (`POP = 1`, `PUSH = 2`, `MOVE = 3`).

Лицензия: MIT (по комментарию в файле).
Переносимость: высокая.

### 2.3. `lib/memory.lua`

Подтверждённые функции/константы:
- `page_access`: `NOACCESS`, `READONLY`, `READWRITE`, `EXECUTE`, `EXECUTE_READ`, `EXECUTE_READWRITE`.
- `memory.read(address, size, unprot)` — чтение до 8 байт.
- `memory.write(address, value, size, unprot)`.
- `memory.unprotect`, `memory.protect`, `memory.copy`, `memory.fill`, `memory.tostring`, `memory.compare`, `memory.strptr`, `memory.tohex`, `memory.hex2bin`.
- `memory.getvalue`/`setvalue` и типизированные геттеры/сеттеры: `getint8/16/32/64`, `getuint8/16/32/64`, `getfloat`, `getdouble` и соответствующие `set*`.
- `memory.pageaccess = page_access`.

Использует `ffi` (`ffi.cdef` для `mprotect`, `memcmp`), `bit`, `ffi.cast`, `ffi.new`, `ffi.copy`, `ffi.fill`, `ffi.string`.

Android API: нет прямого JNI; использует POSIX-подобный `mprotect`.
FFI: да.
Нативные библиотеки: нет (только libc через FFI).
Переносимость на iOS: частичная — `mprotect` доступен, но адреса/offsets/защита памяти iOS отличаются, JIT-права ограничены.

### 2.4. `lib/imgui.lua`

Содержимое:
```lua
return require("mimgui.compat_imgui")
```
Модуль-обёртка над `mimgui`.

### 2.5. `lib/mimgui/`

Подтверждено:
- `mimgui/init.lua`: `_VERSION = "2.0.1"`.
- Использует `ffi`, `bit`.
- Требует `mimgui.cimgui`.
- Определяет имя скрипта через `thisScript`.
- Пытается вызвать `ffi.C.neom_mimgui_create_context(script_name)` — нативная функция NeoLoader.
- Если вызов не удался, использует `ffi.C.igGetCurrentContext()` / `igCreateContext(nil)`.
- `ffi.C` указывает на глобальный C namespace LuaJIT, следовательно модуль зависит от нативной библиотеки NeoLoader, экспортирующей `neom_mimgui_create_context` и связанные `neom_mimgui_*` функции (см. раздел 4).

Файлы: `mimgui/cimgui.lua` (483 321 байт), `mimgui/cdefs.lua` (115 206), `mimgui/enums.lua` (33 424), `mimgui/imgui.lua` (101 487), `mimgui/compat_imgui.lua` (1 801), шрифт `trebucbd.ttf` (244 124), `mimgui/init.lua` (116 071).

Отдельно `lib/mimgui/imgui.lua` использует `ffi.load("monetloader")` для ImGui-символов (`ig*`).

Android API: нет прямого JNI.
FFI: да.
Нативные библиотеки: да (через `ffi.C` и `ffi.load`).
Переносимость: низкая без iOS-аналога ImGui и нативного контекста.

### 2.6. `lib/SAMemory/` и `lib/game/`

Подтверждено:
- `SAMemory/init.lua` использует нативный модуль `NeoSAMemory` (`require 'NeoSAMemory'`), `shared.ffi`, читает `MONET_GTASA_BASE`.
- `SAMemory/metatype.lua`, `shared.lua`, `vector2d/3d.lua`, `quaternion.lua`, `matrix.lua`.
- `SAMemory/game/*` — 93 файла, описывают структуры GTA SA: `CCam`, `CCamera`, `CEntity`, `CObject`, `CPed`, `CVehicle`, `CPlayerPed`, `CWeapon`, `CWeaponInfo`, `CPhysical`, `CPlaceable`, `CPool`, `CPlayerInfo`, `CWanted`, `CTask`, `CFire`, `CHeli`, `CPlane`, `CTrain`, `CBoat`, `CBike`, `CBmx`, `CAutomobile`, `CDoor`, `CDamageManager`, `CObjectInfo`, `CPlayerData`, `CPedIntelligence`, `CPedIK`, `CPedClothesDesc`, `CEvent*`, `CCol*`, `CAE*` аудио-сущности, `RenderWare`, `eCamMode`, `ePedState`, `ePedType`, `eVehicleHandlingFlags`, `eWeaponType`, `tHandlingData`, `tBoatHandlingData`, `tFlyingHandlingData`, `tTransmissionGear`, `FxSystem_c`, `quaternion`, `vector2d/3d`, `matrix`.
- `game/`: `globals.lua`, `models.lua`, `weapons.lua`, `keys.lua`.

Назначение: работа с памятью игры через offsets.
Android API: косвенно (offsets под Android `libGTASA.so`).
FFI: да (через `NeoSAMemory` и `ffi`).
Нативные библиотеки: зависит от `libGTASA.so` и NeoLoader native bridge.
Переносимость на iOS: не подтверждена; offsets и бинарь отличаются.

### 2.7. `lib/samp/`, `lib/sampfuncs.lua`, `lib/RakLua.lua`

Подтверждено:
- `samp/events.lua` — константы RPC/пакетов, маппинг на `on*` callbacks.
- `samp/events/core.lua` — диспетчер `onSendRpc`/`onSendPacket`/`onReceiveRpc`/`onReceivePacket`, `process_packet`, чтение/запись bitstream.
- `samp/events/handlers.lua`, `bitstream_io.lua`, `utils.lua`, `extra_types.lua`, `core.lua.bak_mh`.
- `samp/raknet.lua` (11 714), `samp/synchronization.lua` (4 452).
- `sampfuncs.lua` (14 501) — константы RPC/packets/gamestate и глобальная регистрация (`for k,v in pairs(sampfuncs) do _G[k]=v end`).
- `RakLua.lua` (780) — хукает `addEventHandler` для `onSendRpc`/`onSendPacket`/`onReceiveRpc`/`onReceivePacket`, делегирует в `RakLuaSO` (нативный модуль, не найден в поставке).
- `arzmod.lua` использует `lib.samp.events` и переопределяет `sampGetPlayerIdByCharHandle`, `sampGetCharHandleBySampPlayerId`, `sampGetPlayerNickname`.

Назначение: SA-MP events, RakNet, синхронизация.
Android API: зависит от `libarzmod.so`/SAMP.
FFI: вероятно (через `RakLuaSO`).
Переносимость: низкая без iOS-клиента с тем же SAMP.

### 2.8. `lib/android/`

Подтверждённые файлы:
- `android/arizona.lua` (1 427) — внедряет `arzapi.jar` через JNI, экспортирует `closeCurrentDialog`, `createNotification`, `sampCloseCurrentDialogWith`, `sampCloseCurrentDialogWithButton`.
- `android/jni-raw.lua` (25 135) — полные FFI-определения JNI-интерфейса; использует `NeoMLoaderGetJNIEnv`/`NeoMLoaderGetActivity` (native NeoLoader).
- `android/jnienv.lua` (64 927) — обёртки над JNIEnv (FindClass, Call*Method, NewGlobalRef и т.д.).
- `android/jnienv-util.lua` (14 759) — утилиты (`InjectJar`, `CallStaticVoidMethod`, `ActivityClassloader`, `FindClass` и др.).
- `android/jar/arzapi.jar` (1 762) — JAR для Arizona API (classes.dex, 2 828 байт).
- `android/jar/` также содержит `WebViews.jar`? Нет, `WebViews.jar` в `lib/webviews/`.

Android API: да, полностью JNI.
FFI: да.
Нативные библиотеки: Android runtime + native NeoLoader (`NeoMLoaderGetJNIEnv`/`NeoMLoaderGetActivity`).
Переносимость: не переносится напрямую; требует отдельной iOS-реализации через нативный bridge.

### 2.9. `lib/MoonMonet/`

Подтверждено:
- `MoonMonet/init.lua` использует `ffi.load(getWorkingDirectory() .. "/lib/MoonMonet/libmoonmonet.so")` и вызывает `moonmonet_build_colors`.
- Экспортируемая функция: `moonmonet_build_colors(float chroma_multiplier, uint32_t color, bool accurate_shades)` → структура `Colors` (accent1/2/3, neutral1/2; каждая SubColors с color_0…color_1000).
- `MoonMonet.buildColors(color, chroma_multiplier, accurate_shades)`.

Нативные библиотеки: Android ELF (3 файла: `libmoonmonet.so`, `libmoonmonet_rs_arm64.so` — 505 624 байт, `libmoonmonet_rs_arm.so` — 118 968 байт).
Переносимость: низкая без пересборки под iOS или замены (см. `docs/NATIVE_LIBRARY_ANALYSIS.md`).

### 2.10. `lib/webviews/`

Подтверждено:
- `webviews/init.lua` внедряет `WebViews.jar` через JNI (`InjectJar(..., "WebViews/Main", "init", "(Landroid/app/Activity;)V")`).
- Экспортирует ~30 функций: `createBrowser`, `createBrowserFromFile`, `executeJS`, `changeUrl`, `setClickable`, `setVisible`, `setPos`, `setSize`, `deleteBrowser`, `showCloseButton`, `setSetting`, `addLog`, `sendClick`, `resetPort`, `getPort`, `canGoForward/Back`, `goForward/Back`, `getJSValue`, `getStatus`, `getVersion`, `getBrowsers`, `getBrowser`, `getScreen`, `onAction`, `restartCallback`.
- Использует UDP-сокет на localhost для callback-ов (`UDP:setpeername("127.0.0.1", port)`), `lua_thread.create`, `wait`.
- Зависит от `cjson`, `socket`, Android JNI.

Назначение: веб-представления (Android WebView).
Android API: да.
Переносимость: низкая; на iOS требуется нативный WebView (WKWebView) bridge.

### 2.11. `lib/socket/`, `lib/ssl/`, `lib/copas/`, `lib/requests.lua`, `lib/snet/`, `lib/ltn12.lua`, `lib/mime.lua`

Подтверждено:
- `socket/`: `http.lua`, `ftp.lua`, `smtp.lua`, `url.lua`, `headers.lua`, `tp.lua`, `mbox.lua`; `socket.lua` (корневой) — LuaSocket.
- `ssl/`: `https.lua`; `ssl.lua` — LuaSec.
- `requests.lua` — HTTP-клиент (GET/POST/PUT/DELETE/PATCH/OPTIONS/HEAD/TRACE), digest/basic auth, multipart.
- `copas/` + `copas.lua` — асинхронное сетевое программирование (http, ftp, smtp, timer, queue, semaphore, lock).
- `snet/` + `snet.lua` — "Simple Network Module v.1.0.1" (UDP с подтверждением доставки, приоритеты).
- `scriptmgr.lua` использует `socket.http`, `ltn12`, `effil` (потоки).

Назначение: HTTP/TCP/UDP/SSL.
Android API: нет прямого JNI (POSIX-сокеты).
FFI: возможно (LuaSocket/LuaSec).
Переносимость: высокая/средняя при наличии LuaSocket/LuaSec под iOS. Зависимости и лицензии требуют проверки.

### 2.12. `lib/jit/`

Подтверждено: LuaJIT bytecode/disassembler (`bc.lua`, `dump.lua`, `bcsave.lua`, `p.lua`, `v.lua`, `zone.lua`, `dis_arm.lua`, `dis_arm64.lua`, `dis_arm64be.lua`, `dis_mips*.lua`, `dis_ppc.lua`, `dis_x64.lua`, `dis_x86.lua`).
Назначение: отладка/дизассемблирование.
Переносимость: зависит от LuaJIT на iOS.

### 2.13. `lib/cjson/`, `lib/dkjson.lua`, `lib/jsoncfg.lua`

- `cjson/` содержит только `util.lua`; сам `cjson` — нативный модуль (C), не найден в поставке как `.lua`.
- `dkjson.lua` (23 790) — чистый Lua JSON.
- `jsoncfg.lua` — чтение/запись JSON-конфигов в `config/`, пути с разделителем `/` или `\` в зависимости от `MONET_VERSION`.

### 2.14. `lib/scriptmgr.lua` (менеджер скриптов)

Подтверждённые зависимости:
`moonloader`, `widgets`, `mimgui`, `fAwesome6`, `jsoncfg`, `cjson`, `memory`, `lfs`, `effil`, `socket.http`, `ltn12`, `copas`, `copas.http`, `android.jnienv` (опционально), `android.jnienv-util` (опционально), `webviews` (опционально).

Подтверждённые функции:
- `prettyPrint`, `prettyPrintTable`.
- `circular_buffer` (push/clear/reverse_iter/reverse_ipairs/filled).
- `locales.get/set_lang/get_languages`.
- `formatClock`.
- `httpRequest`, `downloadToFile`, `openLink`, `isAllowedUrl`.
- `disableScript`, `enableScript`, `getDisabledScripts`, `unloadScriptByName`.
- `getRealPath`.
- `onScriptCrashed`, `onScriptMessage`, `onSystemMessage`, `onScriptTerminate`, `onScriptLoad`.
- `main()`.

Назначение: менеджер скриптов, UI (ImGui), загрузки, локализация, логирование.
Android API: косвенно (пути, UI, WebViews).
Переносимость: частичная; требует iOS-аналогов путей, UI, потоков, WebViews.

### 2.15. Прочие модули

- `lib/bass.lua` — FFI-обёртка над BASS (аудио); использует нативные `NeoMLoader_GetBassPath`/`NeoMLoader_GetBassSymbol` (NeoLoader) и `ffi.C.BASS_Init`. Зависит от NeoLoader native.
- `lib/monethook.lua` — хуки через `monethook_hook_enable/disable` (требует MonetLoader 2.8.0+); хукирует `onScriptTerminate`.
- `lib/sa_renderfix.lua` — исправление рендера (MonetLoader); изменяет `setTextScale` и др.
- `lib/timerwheel.lua`, `lib/binaryheap.lua` — планировщик/таймеры.
- `lib/encoding.lua` — кодировки (cp1251↔utf8 и др.).
- `lib/base64.lua`, `lib/basexx.lua`, `lib/md5.lua`, `lib/sha1/`, `lib/sha256.lua` — кодирование/хэши.
- `lib/ltn12.lua`, `lib/mime.lua` — потоки/MIME.
- `lib/inicfg.lua`, `lib/jsoncfg.lua` — конфиги.
- `lib/vkeys.lua` — коды клавиш.
- `lib/widgets.lua` — константы виджетов (WIDGET_*).
- `lib/fAwesome5.lua`, `lib/fAwesome6.lua`, `lib/fAwesome6_solid.lua` — иконки FontAwesome.
- `lib/imgui_piemenu.lua` — pie-меню для ImGui.
- `lib/matrix3x3.lua`, `lib/vector3d.lua` — математика.
- `lib/bitex.lua` — расширенные битовые операции.
- `lib/LIP.lua` — INI-парсер.

### 2.16. Нативные функции NeoLoader, используемые через FFI

Подтверждено поиском по Lua-исходникам:

- `NeoMLoaderGetJNIEnv()` / `NeoMLoaderGetActivity()` — JNI environment/Activity (используется в `android/jni-raw.lua`).
- `NeoMLoader_GetBassPath()` / `NeoMLoader_GetBassSymbol(const char*)` — путь к BASS и получение символа (используется в `bass.lua`).
- `neom_mimgui_create_context(script_name)` и `neom_mimgui_*` (create_texture_from_file, create_texture_from_memory, get_dpi_scale, invalidate_fonts_texture, push/pop button_repeat, push/pop_font, pop_style_color, pop_style_var, release_texture, switch_context) — ImGui-контекст (используется в `mimgui/init.lua` и `mimgui/cimgui.lua`).
- `ffi.load("GTASA")` / `ffi.load("monetloader")` — загрузка игровой/monet-библиотеки как глобального C-namespace.
- `ffi.load(getWorkingDirectory() .. "/lib/MoonMonet/libmoonmonet.so")` — MoonMonet.

## 3. Неизвестные детали

- Точная сигнатура всех функций `memory.lua` выписана (раздел 2.3).
- Полный список экспортов `mimgui` не выписан (большие файлы `cimgui.lua` 483 321 байт, `cdefs.lua` 115 206).
- Полный набор функций `sampfuncs.lua` выписан (константы), но функции (не константы) не подтверждены — файл содержит только константы и глобальную регистрацию.
- Назначение `webviews/` подтверждено (раздел 2.10).
- Нативные функции NeoLoader (`neom_mimgui_create_context` и др.) подтверждены по имени; их реализация в `.so` не найдена (доступны только MoonMonet ELF; `libneomloader.so` отсутствует).
- Внутренние детали загрузчика (как именно подключается LuaJIT, как внедряется в игру) не подтверждены исходниками.
- `RakLuaSO` (нативный модуль) не найден в поставке.
- `NeoSAMemory` (нативный модуль) не найден в поставке.
- `effil` (потоки) не найден как файл — предположительно нативный модуль NeoLoader.
- `mime.core` (нативный) не найден как файл.
- `lua_thread`, `wait`, `doesFileExist`, `getGameDirectory`, `thisScript`, `script.this` — глобальные функции NeoLoader runtime, их реализация не в Lua.
- `MONET_VERSION`, `MONET_GTASA_BASE`, `PLAYER_PED` — глобальные, инжектируемые runtime/игрой.
- Лицензии LuaJIT/ImGui/BASS и прочих не проверены (файлы лицензий в поставке не найдены).
- `libneomloader.so` и `libag-client.so` не найдены в доступной среде.

## 4. Источники и метод

- Чтение Lua-файлов через `cat`/`sed`/`grep` (без выполнения).
- `file`, `readelf -d`, `nm -D` для трёх MoonMonet ELF (без запуска).
- `unzip -l` для JAR (без извлечения/запуска).
- Подсчёт файлов через `find`.
- Git-репозиторий ArizonaNeo-iOS не содержит исследовательских материалов; они лежат вне репозитория.
- MonetLoader (`com.arizona.game`) использовался только для сравнения имён/констант, не как замена NeoLoader.
