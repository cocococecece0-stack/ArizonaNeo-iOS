# Stage 12.0 — Arizona In-Process Bootstrap Integration

## Результат
**A. PACKAGING_PASS** и **B. MACHO_LINK_PASS**. **C. DEVICE_LOAD_PASS** и **D. RUNTIME_READY** НЕ заявляются: IPA не устанавливался и dylib не загружался внутри Arizona на устройстве.

## Проверенная dylib
- Источник: CI artifact run `45641509524`, файл `build-core/libNeoLoaderBootstrap.dylib`.
- SHA256: `c74acb30deaac7846a1e3bf0d4a2a0407fa029a8d7799355fa4eafcea71b83c9`.
- `file`: Mach-O 64-bit arm64 dynamically linked shared library.
- `LC_BUILD_VERSION`: platform 2 (iOS), minos 15.0, sdk 18.5.
- `LC_ID_DYLIB`: `@rpath/libNeoLoaderBootstrap.dylib`.
- Зависимости только системные: `/usr/lib/libSystem.B.dylib`, `/usr/lib/libc++.1.dylib`. Android/Bionic/JNI-зависимостей нет.
- Экспорты: `neo_bootstrap_initialize`, `neo_bootstrap_shutdown`, `neo_bootstrap_get_status`, `neo_bootstrap_get_version`, `neo_bootstrap_attach_host`.
- Core статически включён в dylib через `libarizona_neo_core.a` (не динамическая зависимость).

## Оригинальный IPA
- `/root/ArizonaNeomios/inputs/Arizona-iOS-17.9.3-9281672.ipa`, notification `dev.local.arizona`, executable `Arizona`, arm64, iOS.
- Использована отдельная копия; оригинальный IPA не изменён.

## Инструмент упаковки
`tools/ipa_integration/integrate_ipa.py`:
- распаковывает IPA во временную директорию;
- находит `.app` по `CFBundleExecutable`;
- копирует dylib в `Arizona.app/Frameworks/`;
- безопасный Mach-O injection: добавляет `LC_LOAD_DYLIB @rpath/libNeoLoaderBootstrap.dylib` и `LC_RPATH @executable_path/Frameworks` в свободное zero-filled пространство Mach-O header, не сдвигая секции;
- проверяет, что первая секция (`__text`) не изменилась и что новые команды read-back прочитаны корректно;
- не патчит инструкции, не перезаписывает существующие load commands;
- создаёт отдельный unsigned IPA.

`tools/ipa_integration/audit_ipa.py` проверяет ZIP integrity, наличие dylib, SHA256, `LC_LOAD_DYLIB`, `LC_RPATH`, Info.plist, executable и явно не заявляет device_load/runtime_ready.

## Фактические показатели интеграции
- `ncmds`: 60 → 62; `sizeofcmds`: 7464 → 7568; добавлено 104 bytes в header padding (`0x1d48..0x4000`).
- Integrated IPA SHA256: `94c3f065535d0f3dd0c818cc0fae5aedaafa30d2c1bfd896c4fde0c2c453a5fe`.
- Audit: `AUDIT_PASS` (все 8 проверок).
- Отчёты: `integration-report.json`, `audit-report.json` в `/root/ArizonaNeo-research/stage-12.0/`.

## Диагностика загрузки
Bootstrap из Stage 11.0已于 изменяется: `neo_bootstrap_get_status()` возвращает state, version, shutdown flag и runtime/game capability флаги. `BOOTSTRAP_READY_HOST_PENDING` означает, что portable runtime готов, а game capabilities недоступны. Для однозначного факта загрузки внутри процесса нужен вызов C ABI из in-process хоста; без установки на устройство这样的证明 impossibility. Фиктивный game callback не создавался.

## Подпись
Подпись изменённого executable недействительна; IPA подготовлен для ESign. Никакие сертификаты не использовались.

## Regression
- Linux Debug/Release + LuaJIT ON: 13/13 PASS (добавлен `ipa_integration_tool_test`).
- Linux LuaJIT OFF: 10/10 PASS.

## CI
- Workflow `.github/workflows/ios-integrated-ipa.yml`: получает оригинальный IPA как отдельный artifact, скачивает строго Stage 11.0 dylib, проверяет SHA256, запускает интеграцию и audit, публикует unsigned IPA + отчёты.
- Локально интеграция и audit выполнены с доступным оригинальным IPA.

## ESign инструкция
1. Скачать `ArizonaNeo-integrated-unsigned.ipa`.
2. Подписать через ESign профилем, покрывающим `dev.local.arizona`.
3. Установить на iPhone.
4. Проверить, что Arizona запускается и dylib загружен; вызвать диагностику `neo_bootstrap_get_status()` через in-process host или проверить логи.
5. Ожидаемое состояние: `BOOTSTRAP_READY_HOST_PENDING`, `game_capabilities_available=0`.
6. При краше прислать crash log — анализировать причину, не заявлять PASS.

## Git
Commit `deb12f3` — `feat(ios): integrate bootstrap dylib into original Arizona IPA`; branch `stage-7.1.7-device-validation`; `main` не изменён.

## Следующий шаг
Получить реальный device report с признаком загрузки dylib и состоянием bootstrap; только после этого заявлять C/D.
