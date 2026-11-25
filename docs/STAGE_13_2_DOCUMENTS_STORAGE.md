# Stage 13.2 — User-accessible NeoLoader Documents Storage

## Что действительно реализовано
- `UserDocumentsRoot` (core): представляет пользовательскую папку `Documents/NeoLoader`, создаёт структуру `scripts/`, `lib/`, `config/`, `logs/`, `resources/`, хранит bookmark и SandboxedFilesystem.
- `DocumentsAccess` / `BookmarkStore` — abstracts host bridge. На iOS их должен реализовать production host через `UIDocumentPickerViewController`, `bookmarkDataWithOptions` и `startAccessingSecurityScopedResource`. Core не импортирует UIKit/Foundation.
- `select()` проверяет абсолютный путь, каталог, создаёт bookmark и structure; `restore()` восстанавливает security scope из bookmark, `persist()` сохраняет его, `release()` снимает доступ.
- При утерянном разрешении `restore()` возвращает ошибку — host обязан снова показать picker.
- `module_candidates()` реализует `lib/?.lua` и `lib/?/init.lua` (плюс fallback без `lib/`).
- `NeoLoaderHost::attach_documents()` подключает выбранный каталог к `require` в `LuaJITHost`; `discover_scripts()` находит `scripts/*.lua`.
- Пути остаются внутри выбранного каталога: path traversal, абсолютные пути, symlink escape отклоняются `SandboxedFilesystem`.
- Пользовательские скрипты НЕ копируются во внутренний контейнер Arizona.

## Интерфейсы, которые отсутствуют
- Нет production entrypoint в original Arizona iOS, из которого можно вызвать `UIDocumentPickerViewController`.
- Нет ObjC/Swift host code, который реализует `DocumentsAccess`/`BookmarkStore` и вызывает `UserDocumentsRoot::select()/restore()`.
- Нет способа без исходников Arizona безопасно добавить кнопку/экран выбора папки. Тестовый UIKit host не создавался.
- Bootstrap constructor остаётся минимальным; Lua runtime не запускается автоматически из-за отсутствия host integration.

## Тесты
- Linux Debug + LuaJIT ON: 14/14 PASS.
- Linux Release + LuaJIT ON: 14/14 PASS.
- LuaJIT OFF: 11/11 PASS.
- iOS ARM64 build через `ios-device-runtime.yml` run `45685236878`: success (Core + dylib собираются из текущего checkout).
- Проверено: выбор каталога, создание структуры, persist/restore bookmark, потеря разрешения, чтение/запись, path traversal, discovery `scripts/*.lua`, require из `lib/?.lua` и `lib/?/init.lua`, выполнение пользовательского Lua-файла, stack hygiene.
- Linux-тесты НЕ проверяют реальный iOS Files picker и security-scoped URLs.

## IPA
- Новый IPA не собирался: UIKit picker не подключён к Arizona, а Stage 13.1 bootstrap не менялся. Используется ранее собранный `/root/ArizonaNeo-research/stage-13.1/ArizonaNeo-integrated-unsigned.ipa`, SHA256 `0de8c49bf32c9bcf050fc29f317ec4b1dbccffe72352d450b9ec588f198c6f7c`.
- Dylib SHA256: `5f9c2710ad36b98e700194d77efbaa35287571741cac500ac6608305edc31073`.
- DEVICE_LOAD_PASS и RUNTIME_READY не заявляются.

## Ограничения
- Выбор папки возможен только после появления production host caller.
- `UserDocumentsRoot` ожидает, что host уже получил security-scoped URL и превратил его в путь; без этого `select()` не вызывается.
- bookmarks хранятся в абстрактном store; на iOS требуется NSUserDefaults + bookmark data.
- Скрипты из `scripts/` загружаются только если runtimebootstrap явно вызвал `attach_documents()` и `discover_scripts()`.

## Инструкция для проверки на iPhone (после появления host integration)
1. Подписать Stage 13.1 unsigned IPA через ESign профилем `dev.local.arizona`.
2. Запустить Arizona; если реализован picker — выбрать папку `Documents/NeoLoader` через Files.
3. Убедиться, что созданы `scripts/`, `lib/`, `config/`, `logs/`, `resources/`.
4. Положить `scripts/test.lua` и `lib/mymod.lua` через Files, перезапустить приложение.
5. Проверить marker `Library/Caches/neo_bootstrap.marker` на `DYLIB_LOADED`/`HOST_PENDING`.
6. Только после реального caller'а проверить `neo_bootstrap_get_status()` → `BOOTSTRAP_READY_HOST_PENDING`.
7. Не заявлять RUNTIME_READY без attach_host с подтверждённым function pointer.

## Git
Branch `stage-7.1.7-device-validation`, commit `436b9d3` — `feat(runtime): add user-selected NeoLoader documents storage`; `main` не изменён, пользовательские документы сохранены.

## Следующий шаг
Получить production host entrypoint в Arizona (ObjC/Swift bootstrap), который реализует `DocumentsAccess`/`BookmarkStore`, вызывает `UIDocumentPickerViewController` и связывает выбранную папку с `NeoLoaderHost::attach_documents()`. Только после этого возможна реальная проверка через Files на iPhone.
