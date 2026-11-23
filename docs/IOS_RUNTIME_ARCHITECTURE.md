# iOS Runtime Architecture (Stage 4)

Статус: проектная архитектура (не реализована). Основана на статическом анализе NeoLoader 1.0.72 и ограничениях iOS 15+/ARM64. C++-реализация не изменялась.

## 1. Цели

- Обеспечить NeoMLoader-совместимую Lua-среду на iOS 15+/ARM64.
- Сохранить переносимую часть (чистый Lua, стандартные библиотеки, файловая система sandbox, сеть, таймеры, script manager, логирование, конфиги).
- Изолировать платформенно-зависимые части (игра, SA-MP, память, JNI, ImGui, WebViews) за абстрактными интерфейсами.
- Не обещать внедрение в процессы/хуки на iOS без jailbreak.

## 2. Слои

```
┌──────────────────────────────────────────────┐
│ Lua-скрипты NeoMLoader (scriptmgr, модули)  │
├──────────────────────────────────────────────┤
│ Compatibility shim (константы, пути, глоб.)  │  ← moonloader/monetloader константы, widgets, vkeys
├──────────────────────────────────────────────┤
│ Lua Runtime (LuaJIT или совместимый)         │  ← LuaHost interface (include/.../lua_host.hpp)
├──────────────────────────────────────────────┤
│ Core services                                │  ← FileSystem, Scheduler, Logger, Config, Net, ScriptManager
├──────────────────────────────────────────────┤
│ Platform adapters (platform/ios/)             │  ← iOS sandbox paths, GCD, NSFileManager, WKWebView, UNUserNotificationCenter
├──────────────────────────────────────────────┤
│ Optional GameBridge (только при наличии IPA) │  ← Game API, SA-MP, Memory, ImGui context
└──────────────────────────────────────────────┘
```

## 3. Компоненты Core (platform-independent)

- `LuaHost` (уже объявлен в `include/arizona_neo/core/lua_host.hpp`): `initialize()`, `execute(source, chunk_name)`, `shutdown()`. Требует расширения: reporting ошибок Lua, обработка длины `string_view`.
- `FileSystem`: абстракция путей (`getWorkingDirectory`, `getGameDirectory`, `doesFileExist`, `lfs`-операции) поверх iOS sandbox (Documents/Library/Caches).
- `Scheduler`: таймеры (`timerwheel`, `binaryheap`), `lua_thread`-подобный API, GCD-бэкенд.
- `Logger`: уровни `TAG` (TYPE_INFO … TYPE_EXCEPTION), файловые логи в sandbox.
- `Config`: `jsoncfg`/`inicfg` с путями sandbox.
- `Net`: LuaSocket/LuaSec или Network.framework, `requests`, `copas`, `snet`.
- `ScriptManager`: загрузка/выгрузка скриптов, disabled-scripts список, `onScriptLoad/Terminate/Crashed/Message`, `main()`.

## 4. Platform adapters (platform/ios/, не созданы)

- `IOSPathProvider`: возвращает sandbox-пути, аналог `getWorkingDirectory/getGameDirectory`.
- `IOSScheduler`: GCD-таймеры, фоновые очереди (ограничения iOS).
- `IOSFileSystem`: NSFileManager-обёртка, права доступа.
- `IOSWebView`: WKWebView bridge (замена Android `webviews/`).
- `IOSNotifications`: UNUserNotificationCenter (замена `android/arizona.lua`).
- `IOSImGuiRenderer`: Metal/ImGui (замена `neom_mimgui_*` и `ffi.load("monetloader")`).

## 5. Compatibility shim

- Константы `moonloader.lua`/`monetloader.lua` переносятся как есть (высокая переносимость).
- Глобальные, инжектируемые runtime (`MONET_VERSION`, `MONET_GTASA_BASE`, `PLAYER_PED`, `thisScript`, `script.this`, `getWorkingDirectory`, `getGameDirectory`, `doesFileExist`, `lua_thread`, `wait`, `addEventHandler`, `NeoMLoaderGetJNIEnv/Activity`) — реализуются через адаптеры или заглушки-ошибки (не фиктивные: явное "not available on iOS" где невозможно).
- Пути: `MONET_VERSION == nil` ветка (Windows-пути `\`) не используется на iOS; используется `/`.

## 6. GameBridge (опциональный, Stage 5+)

- Активируется только при наличии iOS-клиента Arizona RP и подтверждённых точек входа.
- Предоставляет: чтение/запись памяти (через iOS Mach-O layout), Game API, SA-MP, ImGui context, аудио.
- На iOS без jailbreak может быть недоступен; тогда GameBridge возвращает "not available", а не падает.

## 7. Ограничения и честность реализации

- Android ELF (`.so`) не копируются и не загружаются на iOS.
- JNI-слой (`android/`) не переносится; заменяется нейтральным интерфейсом.
- Хуки (`monethook.lua`) не реализуются на iOS без jailbreak.
- FFI к произвольным библиотекам не поддерживается; `ffi.load` работает только для статически связанных/системных.
- LuaJIT JIT-права на iOS не подтверждены; требуется проверка на устройстве.

## 8. Тестирование

- CTest для C++ Core (уже есть `tests/version_test.cpp`).
- Smoke-тесты для путей/конфигов без реального runtime (планируются).
- Каждый этап — отчёт и запуск тестов перед переходом.

## 9. Что не реализовано

- Каталоги `platform/ios/`, `compatibility/`, `tests/` (расширенные) не созданы.
- Lua runtime не подключён (`lua_host.cpp` пуст).
- GameBridge не начат.
- Нет IPA.

## 10. Источники

- `docs/ABI_RESEARCH.md`
- `docs/NEOMLOADER_API_INVENTORY.md`
- `docs/IOS_COMPATIBILITY_MATRIX.md`
- `include/arizona_neo/core/lua_host.hpp`
