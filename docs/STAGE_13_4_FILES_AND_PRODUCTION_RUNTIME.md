# Stage 13.4 — Arizona Files Integration + NeoLoader Production Runtime

## Реализовано
- Интегратор `tools/ipa_integration/integrate_ipa.py` теперь добавляет в Info.plist главного Arizona приложения:
  - `UIFileSharingEnabled = true`
  - `LSSupportsOpeningDocumentsInPlace = true`
- Все остальные ключи Info.plist сохраняются; audit проверяет оба ключа.
- `neo_bootstrap_set_documents_root(path)` добавлен в Bootstrap C ABI: host может привязать `Documents/NeoLoader` к runtime вместо временной директории.
- Stage 13.2 `UserDocumentsRoot`/`attach_documents()`/`discover_scripts()` остаются основой; path traversal и sandbox checks сохранены.
- `neo_ios_documents_host` target собирается в iOS CI (Foundation/UIKit/UniformTypeIdentifiers).

## Что реально доступно пользователю в Files
После ESign-подписи и установки интегрированного IPA пользователь должен видеть:

Files → On My iPhone → Arizona → Documents

При первом запуске runtime должен создать:
Documents/NeoLoader/{scripts,lib,config,logs,resources}

Пользователь может добавлять `.lua` файлы в `scripts/` и модули в `lib/` через Files без ESign.

## Может ли Runtime реально запускаться в Arizona
- Библиотека `libNeoLoaderBootstrap.dylib` теперь имеет `__init_offsets` constructor и загружается dyld при условии, что IPA установлена через ESign и dylib присутствует.
- Constructor фиксирует факт загрузки в marker/os_log и **не** инициализирует тяжёлый runtime.
- `neo_bootstrap_initialize()` должен вызывать in-process host Arizona; production caller всё ещё отсутствует.
- `RUNTIME_READY` возможен только после `neo_bootstrap_attach_host()` с реальным function pointer.
- Поэтому `DEVICE_LOAD_PASS` и `RUNTIME_READY` не заявляются до физического report.

## Как загружаются Lua-скрипты
1. Host вызывает `neo_bootstrap_initialize()`.
2. Host получает путь `Documents/NeoLoader` (внутренний sandbox Arizona, не security-scoped bookmark).
3. Вызывает `neo_bootstrap_set_documents_root(path)`.
4. Вызывает `NeoLoaderHost::attach_documents(root)` и `discover_scripts()`.
5. `PortableScriptLoader` читает `scripts/*.lua` и `lib/?.lua`, `lib/?/init.lua`.
6. `ScriptManager` загружает и исполняет скрипты; ошибки сохраняются в `last_error()`.

## Заблокированные функции
- Нет production entrypoint для вызова `UIDocumentPickerViewController` — пользователь видит Files app, но не системный picker внутри Arizona.
- Game API (GTA/SA-MP), JNI/Bionic/ELF не реализованы.
- `main()` игровых скриптов с game calls не запускается.
- `RUNTIME_READY` недостижим без реального host caller.

## SHA256
- Integrated IPA: `de39850c79ee6f0658e01e6e01affb8aadb512dfe7ad86844a907141c56f42cb`
- Bootstrap dylib: `a70d2d0b968621ce16f4117086115b53a7a96520a0ffcc4e442c45fc312bce58`
- Original IPA: `/root/ArizonaNeomios/inputs/Arizona-iOS-17.9.3-9281672.ipa` (не изменён)

## CI
- Run `45690841586`: success.
- Audit: `AUDIT_PASS` (15 checks, включая `UIFileSharingEnabled`, `LSSupportsOpeningDocumentsInPlace`, `dylib_mod_init_func`).

## Инструкция проверки на iPhone
1. Скачать integrated unsigned IPA.
2. Подписать ESign профилем `dev.local.arizona`.
3. Установить на iPhone.
4. Открыть Files → On My iPhone → Arizona → Documents → убедиться, что есть `NeoLoader/scripts` и т.д.
5. Добавить `scripts/neo_test.lua` и `lib/neomodule.lua` через Files.
6. Запустить Arizona, проверить marker `/Library/Caches/neo_bootstrap.marker` на `DYLIB_LOADED`.
7. Проверить crash log и отсутствие dyld error.
8. Не заявлять DEVICE_LOAD_PASS/RUNTIME_READY без реального report.

## Git
Branch `stage-7.1.7-device-validation`, `main` не изменён. Пользовательские документы сохранены.
