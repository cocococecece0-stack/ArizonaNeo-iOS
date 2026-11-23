# Stage 4 Report — NeoLoader Runtime & API Reverse Engineering

Дата: 2026-11-23.
Репозиторий: `/root/ArizonaNeo-iOS`. Ветка `main`.
Исследовательские материалы: `/root/ArizonaNeo-research` (скопированы из `/mnt/sdcard/Android/media/com.arizona21.game.web/neomloader/`, 300 файлов; не добавлены в Git, не запускались).

## 1. Что проверено

- Git-история и статус репозитория ArizonaNeo-iOS.
- Наличие и содержимое `docs/` (до аудита: только `PORTING_PLAN.md`, `RESEARCH_NOTES.md`; Stage 4-документы отсутствовали, хотя предыдущий отчёт утверждал обратное).
- Доступность `/root/ArizonaNeo-research` (изначально пуста; восстановлена из установленного NeoLoader на sdcard).
- Количество файлов: 300 (210 `.lua`, 3 `.so`), версия `1.0.72` build `172` — подтверждено `version.json`.
- Статический анализ реальных Lua-модулей: moonloader, monetloader, memory, mimgui, SAMemory/game, samp/events, android/*, MoonMonet, webviews, socket/ssl/copas/snet/requests, jit, cjson, scriptmgr и др.
- ELF-метаданные трёх MoonMonet `.so` (`file`, `readelf -d`, `nm -D`).
- JAR-структура `arzapi.jar` и `WebViews.jar` (`unzip -l`).
- C++ Core: сборка и тесты (1 тест, проходит).

## 2. Обнаруженные подсистемы NeoLoader

- Script Manager (`scriptmgr.lua`, 77 716 байт): загрузка/выгрузка скриптов, UI (ImGui), загрузки, локализация, логирование, WebViews.
- Lua-среда: LuaJIT (каталог `jit/`), `require()` для модулей `lib/...`.
- Memory API (`memory.lua`): `mprotect`, чтение/запись, защита страниц.
- ImGui (`mimgui/`): версия 2.0.1, зависит от `ffi.C.neom_mimgui_create_context` и `ffi.load("monetloader")`.
- SAMemory/Game API: структуры GTA SA, offsets, `compat/profile.json`.
- SA-MP API: `samp/`, `sampfuncs.lua`, `RakLua.lua`, `arzmod.lua`.
- Android JNI API: `android/arizona.lua`, `jni-raw.lua`, `jnienv.lua`, `arzapi.jar`.
- MoonMonet: нативная ELF для цветовой палитры (1 экспорт).
- Сеть: `socket/`, `ssl/`, `copas/`, `requests.lua`, `snet/`.
- Файловая система/UI: `lfs`, `widgets`, `webviews`, `imgui_piemenu`.
- Константы: `moonloader.lua`, `monetloader.lua`.
- Нативные функции NeoLoader: `NeoMLoaderGetJNIEnv/Activity`, `NeoMLoader_GetBassPath/Symbol`, `neom_mimgui_*`.

## 3. API, реализуемые на iOS (без GameBridge)

- Чистый Lua и стандартные библиотеки (L0).
- Собственный `require()` и module loader (L1).
- Файловая система sandbox (L1).
- Таймеры/события/scheduler (L1).
- Сеть (HTTP/TCP/UDP) с проверкой зависимостей (L1/L2).
- Script Manager (адаптация) (L1).
- Логирование/диагностика (L1).
- Базовые константы MoonLoader/MonetLoader (L0).
- Кодирование/хэши/INI/JSON-парсеры (L0).

## 4. API, требующие iOS-реализации (L3)

- ImGui (нужен iOS/Metal renderer и нативный контекст).
- WebViews (нужен WKWebView bridge).
- Аудио/BASS (нужен BASS for iOS или AVAudioEngine, проверка лицензии).
- Android JNI API (заменяется нативным bridge).
- Нативные модули: `cjson`, `mime.core`, `effil`, `RakLuaSO`, `NeoSAMemory`.

## 5. API, невозможные без GameBridge/дополнительных сведений (L4)

- Игровые функции GTA SA.
- SA-MP events/RakNet.
- Memory offsets и хуки.
- ImGui context, зависящий от renderer/игры.
- Внедрение в игру/процессы.
- Реальные экспорты NeoMLoader (бинарник недоступен).
- iOS-специфичные offsets и символы.
- FFI к игровым функциям.
- JIT-права на iOS (не подтверждено).
- Хуки (`monethook`) без jailbreak.

## 6. Созданные/обновлённые документы

- `docs/NEOMLOADER_API_INVENTORY.md` — инвентарь API NeoLoader (восстановлен и дополнен).
- `docs/IOS_COMPATIBILITY_MATRIX.md` — матрица совместимости Android→iOS.
- `docs/IOS_RUNTIME_ARCHITECTURE.md` — архитектура iOS runtime.
- `docs/SCRIPT_COMPATIBILITY_LEVELS.md` — уровни совместимости скриптов (L0–L4).
- `docs/NATIVE_LIBRARY_ANALYSIS.md` — анализ нативных библиотек.
- `docs/STAGE_4_REPORT.md` — этот отчёт.
- `docs/PORTING_PLAN.md` — обновлён (Stage 4 expanded).
- `docs/PROJECT_AUDIT.md` — обновлён (ссылки и статус).

## 7. Следующий этап

Stage 5: реализация переносимой части (файловая система, логирование, планировщик, script manager) и расширение тестов, без LuaJIT и без фиктивного runtime.
Игровой bridge — только после получения iOS-клиента/бинарников и подтверждённых точек входа.

## 8. Ограничения и честность

- Исследовательские материалы не копировались в Git.
- Lua-скрипты и Android ELF не запускались.
- C++-реализация не изменялась.
- Commit/push не выполнялись.
- Отсутствующие материалы (iOS IPA, `libneomloader.so`, `libag-client.*`) зафиксированы, а не скрыты.
- Лицензии сторонних компонентов не проверены (файлы лицензий в поставке не найдены).
