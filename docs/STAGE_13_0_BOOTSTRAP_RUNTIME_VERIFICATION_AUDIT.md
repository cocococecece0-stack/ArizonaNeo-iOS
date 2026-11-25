# Stage 13.0 — Bootstrap Runtime Verification Audit

## Часть 1. Preflight
- Branch: `stage-7.1.7-device-validation`
- HEAD: `ad1493c08b1776806c7c02762135ff6a2cbe0d21`
- `git status --short`: только `docs/LUA_RUNTIME_RESEARCH.md` и `docs/STAGE_6_2_6_GITHUB_IOS_CORE_BUILD.md` (пользовательские изменения не тронуты)
- Integrated IPA: `/root/ArizonaNeo-research/stage-12.0/ArizonaNeo-integrated-unsigned.ipa`, SHA256 `94c3f065535d0f3dd0c818cc0fae5aedaafa30d2c1bfd896c4fde0c2c453a5fe`
- Stage 11 dylib: `/root/ArizonaNeo-research/stage-11.0/artifacts/ios-device-luajit-test-host/build-core/libNeoLoaderBootstrap.dylib`, SHA256 `c74acb30deaac7846a1e3bf0d4a2a0407fa029a8d7799355fa4eafcea71b83c9`

## Часть 2. Исследование Bootstrap (фактический код)
### Цепочка вызовов при загрузке dylib через dyld
1. dyld читает `LC_LOAD_DYLIB @rpath/libNeoLoaderBootstrap.dylib` из `Arizona`.
2. Разрешает install name через `LC_RPATH @executable_path/Frameworks` → `Payload/Arizona.app/Frameworks/libNeoLoaderBootstrap.dylib`.
3. Грузит dylib и разрешает её зависимости: только `/usr/lib/libSystem.B.dylib` и `/usr/lib/libc++.1.dylib` (Android/Bionic/JNI真正意义上的 отсутствуют; undefined символов — libc++/Unwind, ожидаемые).
4. **Важно:** в dylib отсутствуют `__mod_init_func` и `LC_ROUTINES_64`. Функция `neo_bootstrap_library_loaded()` с `__attribute__((constructor))` не попала в итоговый Mach-O. Следовательно, даже если dyld загрузил dylib, constructor **не выполняется** и `g_library_loaded` остаётся `false`.

### Состояния и кто их меняет
- `NOT_STARTED` — начальное состояние статического `BootstrapState`.
- `neo_bootstrap_initialize()` — единственный переход в `BOOTSTRAP_READY_HOST_PENDING`: создаёт EventBus, Scheduler, ScriptManager, SandboxedFilesystem, при `ARIZONA_NEO_WITH_LUAJIT=ON` создаёт и инициализирует `LuaJITHost`, затем `NeoLoaderHost`. Idempotent для READY-состояний.
- `neo_bootstrap_attach_host()` — единственный переход в `RUNTIME_READY`, но требует реальный function pointer `graphics_failure_set_handler`; без него возвращает `INVALID_ARGUMENT` и capability не повышает.
- `neo_bootstrap_shutdown()` — переход в `STOPPED`, освобождает runtime.
- `neo_bootstrap_get_status()` — только читает состояние; **не** возвращает `library_loaded` (поля в C ABI struct нет).
- `neo_bootstrap_get_version()` — только строка версии.

### Ответы на вопросы
1. Что происходит при загрузке: dyld resolves/load dylib, никакой bootstrap-код не выполняется (constructor отсутствует).
2. Constructor не инициализирует runtime; по факту в бинарнике его нет.
3. `neo_bootstrap_initialize()` должен вызывать in-process host приложения Arizona.
4. `neo_bootstrap_attach_host()` должен вызывать тот же host после получения подтверждённого function pointer игры.
5. Реального production caller этих функций внутри Arizona **нет**: в исходном Mach-O нет строк/символов `neo_bootstrap_*`, а наш dylib добавляет только экспорты.
6. Да — библиотека может быть загружена, но оставаться в `NOT_STARTED` неопределённо долго (именно это и происходит).
7. Необходимые зависимости dylib: `libSystem.B.dylib`, `libc++.1.dylib`; LuaJIT/Core статически встроены.

## Часть 3. Проверка Mach-O
### Integrated executable
- arm64, MH_EXECUTE, platform 2 / minos 15.0 (не изменённые).
- `LC_LOAD_DYLIB @rpath/libNeoLoaderBootstrap.dylib` добавлен (strong, не weak).
- `LC_RPATH @executable_path/Frameworks` добавлен; корректно разрешается относительно `Payload/Arizona.app/Arizona`.
- `ncmds` 60 → 62, `sizeofcmds` 7464 → 7568. Изменения записаны только в zero-filled header padding `[0x1d48, 0x4000)`; байты за пределами Mach-O header (включая `__text`) побайтово совпадают с оригиналом.
- `LC_CODE_SIGNATURE` остался от оригинала и после модификации **недействителен**; ESign должен re-sign.

### Bootstrap dylib
- arm64, MH_DYLIB, platform 2, min iOS 15.0.
- `LC_ID_DYLIB` = `@rpath/libNeoLoaderBootstrap.dylib` — совпадает с `LC_LOAD_DYLIB`.
- Экспортируются все пять C ABI symbolize; неожиданных Android/Bionic/JNI-зависимостей нет.
- `__mod_init_func`/`LC_ROUTINES_64` отсутствуют — constructor не будет выполнен dyld.

### Ограничение
Успешный запуск Arizona **не доказывает** загрузку dylib сам по себе; здесь он лишь косвенно подтверждает, что dyld не отклонил strong dependency. Прямое доказательство требуется изнутри процесса.

## Часть 4. План device diagnostics
| Признак | Самый надёжный способ | Что доказывает | Изменения | Ограничения | Получение без Mac |
|---|---|---|---|---|---|
| DYLIB_LOADED | Инструментация constructor: `os_log`/файл-маркер в constructor + повторная сборка dylib | Прямой факт выполнения кода dylib в процессе | Требуется пересобрать dylib (изменится SHA256) | Нельзя на текущем артефакте; constructor сейчас отсутствует | Просмотр лог-файла через Files/ESign file manager |
| DYLIB_LOADED (текущий IPA) | Установить IPA через ESign, убедиться, что приложение запускается без dyld error | Косвенно: strong `LC_LOAD_DYLIB` не может быть проигнорирован | Нет | Не прямое доказательство; crash logпри может отсутствовать | ESign install + скриншот/crash log |
| BOOTSTRAP_INITIALIZED | Вызвать `neo_bootstrap_get_status()` из in-process хоста и получить state ≠ `NOT_STARTED` | Прямой факт инициализации | Нужен вызывающий код внутри Arizona | Нет production caller; требуется следующий milestone | Экспорт статуса через GUI/файл, если хост реализован |
| BOOTSTRAP_INITIALIZED (без хоста) | Запись диагностического файла в `neo_bootstrap_initialize()` | Прямой факт вызова initialize | Пересборка dylib | Изменит SHA256 Stage 11 | Файл в container AppData, затем просмотр |
| RUNTIME_READY | Статус `BOOTSTRAP_READY_HOST_PENDING`/`RUNTIME_READY`, `runtime_available=1`, `game_capabilities_available=0` | LuaJIT+Core инициализированы, игра не имитируется | Хост/диагностический экспорт | Требует caller; без attach_host RUNTIME_READY недостижим | Скриншот диагностики |
| os_log | `os_log("neo_bootstrap ...")` из C ABI | Событийный факт | Пересборка dylib | Требует macOS Console или unified log access; на iPhone без Mac — ограничен | Console.app на Mac или syslog-файл, если.accessible |
| Диагностический экран | Существующий подтверждённый UI-хук | Не найден | Неизвестно | Нет подтверждённого способа добавить экран в Arizona | Не применимо |

**Вывод:** на текущем интегрированном IPA можно получить только косвенное DYLIB_LOADED (запуск без dyld error). Для прямых BOOTSTRAP_INITIALIZED и RUNTIME_READY требует нужен следующий этап с инструментированным dylib и in-process caller.

## Подтверждённые факты
- Integrated IPA создан из оригинального без изменения Bundle ID и пользовательских файлов.
- Dylib записана в `Frameworks/`, install name и rpath совпадают.
- Экспорты C ABI присутствуют.
- Orig executable изменён только в header padding; `__text` не тронут.
- Constructor в dylib отсутствует.
- Нет production caller `neo_bootstrap_initialize`/`attach_host`.

## Неподтверждённые предположения
- Что dyld действительно загрузил dylib внутри Arizona (нет прямого marker'а).
- Что `neo_bootstrap_initialize()` когда-либо вызывается.
- Что `RUNTIME_READY` достигнут.
- Что игра не падает по другой причине без crash log.

## Причина отсутствия видимых изменений
Dylib может быть загружена dyld, но: (1) constructor отсутствует/не делает инициализацию, (2) никто не вызывает `neo_bootstrap_initialize()`, (3) никто не вызывает `neo_bootstrap_attach_host()`. Bootstrap остаётся в `NOT_STARTED`, LuaJIT и NeoLoaderHost не создаются. Поэтому UI/поведение Arizona не меняется.

## Production integration blockers
1. Нет in-process caller в исходниках Arizona RP.
2. Нет подтверждённого function pointer для `neo_bootstrap_attach_host()` в доступных артефактах.
3. Текущий dylib не имеет конструкторной диагностики/os_log.
4. C ABI status не экспортирует `library_loaded`.

## Минимальный следующий этап
1. Пересобрать dylib с `__attribute__((constructor))`, гарантированно эмитирующим `__mod_init_func`, и с минимальной диагностикой (os_log и/или файл-маркер в доступном container, например `~/Library/Caches/neo_bootstrap.constructor`).
2. Добавить diag-экспорт, включающий `library_loaded`, `state`, `error`, `shutdown_called`.
3. Предоставить минимальный in-process caller (только после подтверждённого entrypoint в Arizona) или диагностический wrapper, вызывающий `neo_bootstrap_initialize()`.
4. Повторить упаковку IPA, выдать новый SHA256, установить через ESign, собрать лог/файл/скриншот.
5. Только после трёх независимых признаков заявлять DYLIB_LOADED + BOOTSTRAP_INITIALIZED + RUNTIME_READY.

## Вердикт
**READY WITH CONDITIONS.** Текущий IPA пригоден для косвенной проверки загрузки (запуск без dyld error), но прямое доказательство BOOTSTRAP_INITIALIZED/RUNTIME_READY требует инструментированной dylib и in-process caller в следующем этапе. Не заявлять DEVICE_LOAD_PASS или RUNTIME_READY на основании только LC_LOAD_DYLIB.
