# Script Compatibility Levels (Stage 4)

Классификация Lua-скриптов NeoLoader по переносимости на iOS 15+/ARM64.
Основана на статическом анализе исходников (Android). MonetLoader — только архитектурный референс.

## 1. Уровни

| Уровень | Название | Критерии | Примеры |
|---|---|---|---|
| L0 | Pure Lua | Только стандартная библиотека Lua, без FFI/Android/JNI/игровых API | Константы, математика, кодирование |
| L1 | Runtime-dependent | Использует глобальные NeoLoader (`getWorkingDirectory`, `doesFileExist`, `thisScript`, `addEventHandler`, `lua_thread`, `wait`) и/или файловую систему, но без FFI к нативным библиотекам | `scriptmgr`-утилиты, конфиги |
| L2 | FFI-lite | Использует `ffi` для libc/POSIX (`mprotect`, `memcmp`) или чистые C-структуры без игровых offsets | `memory.lua` (частично) |
| L3 | Native-dependent | Зависит от нативных библиотек NeoLoader (`ffi.C.neom_mimgui_*`, `NeoMLoaderGetJNIEnv/Activity`, `NeoMLoader_GetBassPath/Symbol`) или `ffi.load("monetloader"/"GTASA")`, или Android JNI | `mimgui`, `android/*`, `bass.lua`, `webviews` |
| L4 | Game-dependent | Зависит от GTA SA offsets, SAMP, `MONET_GTASA_BASE`, `libGTASA.so`/`libarzmod.so`, хуков | `SAMemory/*`, `game/*`, `samp/*`, `arzmod.lua`, `monethook.lua`, `sa_renderfix.lua` |

## 2. Классификация модулей `lib/`

| Модуль/каталог | Уровень | Обоснование |
|---|---|---|
| `moonloader.lua` | L0 | Константы, побочные эффекты только `require('vector3d'/'matrix3x3')`. |
| `monetloader.lua` | L0 | Только константы touch events. |
| `memory.lua` | L2 | FFI к `mprotect`/`memcmp` (libc). |
| `imgui.lua` | L0 | Re-export. |
| `mimgui/*` | L3 | `ffi.C.neom_mimgui_*`, `ffi.load("monetloader")`, ImGui native. |
| `SAMemory/*`, `game/*` | L4 | GTA SA структуры/offsets, `NeoSAMemory`, `MONET_GTASA_BASE`. |
| `samp/*`, `sampfuncs.lua`, `RakLua.lua` | L4 | SA-MP/RakNet, `libarzmod.so`. |
| `arzmod.lua` (root) | L4 | Переопределяет samp-функции, зависит от `lib.samp.events`. |
| `android/*` | L3 (Android-only) | JNI, `arzapi.jar`, `NeoMLoaderGetJNIEnv/Activity`. На iOS не переносится. |
| `MoonMonet/*` | L3 | `ffi.load(libmoonmonet.so)`, Android ELF. |
| `webviews/*` | L3 (Android-only) | JNI + Android WebView + UDP callback. |
| `socket/*`, `ssl/*`, `https.lua` | L1/L2 | POSIX-сокеты; `mime.core` нативный (L3 для mime). |
| `copas/*` | L1 | Чистый Lua поверх сокетов/таймеров. |
| `requests.lua` | L1 | HTTP поверх `socket.http`. |
| `snet/*` | L1 | UDP поверх `socket`. |
| `cjson/` (util) | L0 | Чистый Lua (util). Сам `cjson` — нативный (L3). |
| `dkjson.lua` | L0 | Чистый Lua JSON. |
| `jsoncfg.lua`, `inicfg.lua` | L1 | Файловая система, `MONET_VERSION`. |
| `base64.lua`, `basexx.lua`, `md5.lua`, `sha1/*`, `sha256.lua` | L0 | Чистый Lua. |
| `encoding.lua` | L0 | Чистый Lua. |
| `ltn12.lua`, `mime.lua` | L1/L3 | `mime.core` нативный. |
| `timerwheel.lua`, `binaryheap.lua` | L0/L1 | Чистый Lua, зависит от `socket.gettime`/`ngx.now`. |
| `bitex.lua` | L0 | Чистый Lua. |
| `bass.lua` | L3 | `NeoMLoader_GetBassPath/Symbol`, `BASS_Init`. |
| `monethook.lua` | L4 | Требует MonetLoader 2.8.0+, хуки. |
| `sa_renderfix.lua` | L4 | MonetLoader render fix (GTA SA). |
| `vkeys.lua`, `widgets.lua` | L0 | Константы. |
| `fAwesome5/6/6_solid.lua` | L0 | Данные иконок. |
| `imgui_piemenu.lua` | L2/L3 | ImGui. |
| `matrix3x3.lua`, `vector3d.lua` | L0 | Математика (Vector3D глобальный). |
| `jit/*` | L2/L3 | LuaJIT-специфика. |
| `LIP.lua` | L0 | INI-парсер. |
| `monethook.lua` | L4 | Хуки. |

## 3. Классификация корневых скриптов

| Скрипт | Уровень | Обоснование |
|---|---|---|
| `arzmod.lua` | L4 | Зависит от `lib.samp.events`, samp-функций. |
| `AutoFlip.lua` | L2–L4 | Не прочитан полностью; имя указывает на gameplay. Требует проверки. |
| `Fishhelper.lua` | L3/L4 | Использует `ffi.load("GTASA")`. |
| `Market_Helper.lua` | L3/L4 | Использует `gta = ffi.load('GTASA')`, WebViews, JNI. |
| `Skip_Notify.lua` | L2–L3 | Не прочитан полностью. |
| `WebMobile.lua` | L3/L4 | Использует `ffi.load('GTASA')`, WebViews. |
| `fix_Alt_button.lua` | L2/L3 | ImGui. |
| `Helper_for_earning_money.lua` | L2–L4 | Не прочитан полностью. |
| `AUTOPILOT_...lua` | L4 | Имя указывает на gameplay. |
| `Chat MentionsV2.lua` | L0 (пустой файл, 0 байт) | Пустой. |

## 4. Уровни совместимости для iOS

- **Совместимо без изменений (L0):** константы, математика, кодирование, INI/JSON-парсеры — при наличии файловой системы sandbox.
- **Требует адаптации (L1–L2):** пути, таймеры, сеть, FFI к libc — при наличии Lua runtime и sandbox.
- **Требует iOS-реализации (L3):** ImGui, WebViews, аудио, JNI-заменители, нативные модули.
- **Не совместимо без GameBridge (L4):** Game API, SA-MP, offsets, хуки.

## 5. Метод определения уровня

1. Поиск `ffi`, `ffi.load`, `ffi.C.` в файле.
2. Поиск Android/JNI (`android.`, `jni`, `InjectJar`, `Activity`).
3. Поиск игровых символов (`GTASA`, `MONET_GTASA_BASE`, `samp`, `SAMemory`, `game.`).
4. Поиск глобальных NeoLoader (`getWorkingDirectory`, `lua_thread`, `wait`, `addEventHandler`).
5. Ручная проверка для крупных файлов (Market_Helper.lua, Fishhelper.lua).

## 6. Ограничения классификации

- Крупные скрипты (Market_Helper.lua ~1.7 МБ) не прочитаны целиком; уровень оценён по фрагментам.
- Уровень L1/L2 для скриптов, использующих `MONET_VERSION`-ветки, зависит от реализации глобальных функций на iOS.
- Лицензии сторонних компонентов не проверены.
- Отсутствие `libneomloader.so`/iOS IPA не позволяет подтвердить нативные экспорты.

## 7. Источники

- `docs/NEOMLOADER_API_INVENTORY.md`
- `docs/IOS_COMPATIBILITY_MATRIX.md`
- `docs/NATIVE_LIBRARY_ANALYSIS.md`
