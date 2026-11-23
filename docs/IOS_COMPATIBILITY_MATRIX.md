# iOS Compatibility Matrix (Stage 4)

Источник: статический анализ NeoLoader 1.0.72 (Android) и ограничения iOS 15+/ARM64.
MonetLoader используется только как архитектурный ориентир, не как замена.
Данные подтверждены чтением Lua-исходников и ELF-метаданных; отсутствующие материалы (iOS IPA, `libneomloader.so`) зафиксированы как неизвестные.

## 1. Общая матрица

| Подсистема | Android API / ожидания Lua-скриптов | Возможный iOS-аналог | Ограничения iOS | Приоритет | Сложность |
|---|---|---|---|---|---|
| Lua interpreter | LuaJIT (по `lib/jit/`) | LuaJIT для iOS ARM64 или совместимый интерпретатор | JIT-права, подпись, sandbox | Высокий | Высокая |
| Module loader (`require`) | Пути `lib/`, `getWorkingDirectory()` | Собственный загрузчик с iOS sandbox paths | Файловая система изолирована | Высокий | Средняя |
| FFI | `ffi.cdef`, `ffi.load`, `ffi.C` | Ограниченно; iOS не позволяет произвольную загрузку нативных библиотек | Code signing, sandbox, отсутствие `.so` | Средний | Высокая |
| Memory API | `mprotect`, offsets GTA SA | POSIX `mprotect` + собственный bridge | Другой адресный layout, защита памяти, нет доступа к памяти игры без jailbreak | Средний | Очень высокая |
| Game API (GTA SA) | Offsets, глобальные переменные | Только при наличии iOS game bridge | Не подтверждено для iOS | Низкий/исследовательский | Очень высокая |
| SA-MP API | События, RakNet, синхронизация | Только при наличии iOS-клиента с SA-MP | Зависит от игры | Низкий/исследовательский | Очень высокая |
| ImGui | `mimgui`, `ffi.C.neom_mimgui_create_context` | ImGui + iOS/Metal renderer | Нужен нативный контекст и ввод | Средний | Высокая |
| События/callbacks | `sampev`, таймеры, `addEventHandler` | Собственный event bus/scheduler | Потоки и runloop iOS | Высокий | Средняя |
| Таймеры/потоки | `effil`, `timerwheel`, `binaryheap`, `lua_thread` | GCD/`NSTimer`/собственный scheduler | Фоновые потоки ограничены | Высокий | Средняя |
| HTTP/TCP/UDP/sockets | LuaSocket/LuaSec, `snet`, `copas` | POSIX sockets / Network.framework | Сеть разрешена, но фоновые ограничения | Средний | Средняя |
| Файловая система | `lfs`, пути Android, `getWorkingDirectory()` | NSFileManager / sandbox dirs | Нет произвольного доступа к файлам | Высокий | Средняя |
| UI/уведомления | Android notifications/dialogs (JNI) | iOS notifications/local UI | Разные API | Средний | Средняя |
| Script Manager | `scriptmgr.lua` | Собственный менеджер | Пути/потоки/UI/WebViews | Высокий | Средняя |
| JNI bridge | `android/jni-*` | Не применимо | iOS не имеет JNI | Н/Д | Невозможно напрямую |
| Аудио (BASS) | `bass.lua`, `NeoMLoader_GetBassPath/Symbol` | AVAudioEngine или BASS for iOS (если лицензия) | Нативная зависимость NeoLoader | Низкий | Высокая |
| WebViews | `webviews/` (Android WebView) | WKWebView | Разные API | Низкий | Высокая |
| Хуки | `monethook.lua` | Не применимо без jailbreak | Ограничения iOS | Н/Д | Невозможно |

## 2. Детальный разбор

### 2.1. Lua interpreter и LuaJIT
- NeoLoader использует LuaJIT (каталог `lib/jit/`, `jit.os`, `jit.arch` проверки в коде).
- На iOS требуется отдельная сборка/проверка LuaJIT ARM64 или совместимый runtime.
- JIT может потребовать специальных прав/подписи; это не подтверждено.
- В коде используется `ffi`, `bit`, `table.new`, `lua_thread`, `wait` — глобальные NeoLoader runtime.

### 2.2. `require()` и загрузчик модулей
- Скрипты используют `require('lib.samp.events')`, `require('mimgui')`, `require('memory')`, `require('jsoncfg')`.
- Нужен собственный загрузчик, учитывающий iOS sandbox и структуру модулей.
- `jsoncfg.lua`/`inicfg.lua` зависят от `MONET_VERSION` (пути `\` vs `/`).

### 2.3. FFI
- `memory.lua` и `mimgui` используют `ffi`.
- `ffi.load("GTASA")`, `ffi.load("monetloader")`, `ffi.load(getWorkingDirectory().."/lib/MoonMonet/libmoonmonet.so")`.
- На iOS произвольная загрузка `.so` недоступна; FFI может работать только для системных/статически связанных библиотек.
- `NeoSAMemory`, `RakLuaSO`, `effil`, `mime.core`, `cjson` — нативные модули, не найденные в поставке.

### 2.4. Работа с памятью
- `memory.read`/`mprotect`/offsets GTA SA.
- POSIX `mprotect` доступен, но offsets и доступ к памяти игры не подтверждены.
- `MONET_GTASA_BASE` инжектируется runtime; на iOS адресный layout другой.

### 2.5. Игровые функции GTA SA
- Зависят от `libGTASA.so` и offsets из `compat/profile.json` (паттерны, offsets).
- На iOS игра имеет другой бинарь; требуется отдельный GameBridge.

### 2.6. SA-MP API
- События, RakNet, синхронизация.
- Требует iOS-клиента с аналогичным SAMP.

### 2.7. ImGui
- `mimgui` зависит от `ffi.C.neom_mimgui_create_context`.
- Нужен iOS renderer и нативный контекст.

### 2.8. События и callbacks
- `sampev`-события, таймеры, `addEventHandler`.
- Реализуемы через собственный event bus.

### 2.9. Таймеры и потоки
- `effil`, `timerwheel`, `binaryheap`, `lua_thread.create`, `wait`.
- Реализуемы через GCD/собственный scheduler, но с ограничениями iOS.

### 2.10. HTTP/TCP/UDP/sockets
- LuaSocket/LuaSec, `snet`, `copas`, `requests`.
- Вероятно переносимы, но зависимости и лицензии требуют проверки.

### 2.11. Файловая система
- `lfs`, `getWorkingDirectory()`, `getGameDirectory()`, пути Android.
- Нужны iOS sandbox paths.

### 2.12. Уведомления и интерфейс
- `android/arizona.lua` создаёт уведомления и диалоги через JNI.
- На iOS нужны UNUserNotificationCenter и нативный UI.

### 2.13. Script Manager и REPL
- `scriptmgr.lua` управляет скриптами, UI, загрузками, WebViews.
- Требует адаптации путей, UI, потоков, WebViews.

### 2.14. Android JNI API
- `android/jni-*` полностью зависит от JNI.
- На iOS не применимо; заменяется нативным bridge.

### 2.15. Аудио (BASS)
- `bass.lua` использует `NeoMLoader_GetBassPath`/`NeoMLoader_GetBassSymbol` и `BASS_Init`.
- На iOS требуется BASS for iOS или AVAudioEngine; лицензия BASS требует проверки.

### 2.16. WebViews
- `webviews/` использует Android WebView через JNI + UDP callback.
- На iOS требуется WKWebView bridge.

## 3. Выводы по достижимости

- Без jailbreak реалистичны: чистый Lua, стандартные библиотеки, файловая система sandbox, сеть, таймеры, собственный UI, script manager.
- Зависят от iOS-клиента игры: Game API, SA-MP, память, хуки, FFI к игровым функциям.
- Не переносится: JNI-слой, хуки (`monethook`), Android WebView.
- Не подтверждено: LuaJIT JIT-права на iOS, BASS, ImGui renderer.

## 4. Источники

- `docs/NEOMLOADER_API_INVENTORY.md`
- `docs/NATIVE_LIBRARY_ANALYSIS.md`
- `docs/ABI_RESEARCH.md`
- `docs/IOS_RUNTIME_ARCHITECTURE.md`
