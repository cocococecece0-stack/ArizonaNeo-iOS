# Stage 15.3.2 — macOS iOS Build Pipeline

Дата: 2026-11-26.

## 1. Preflight

- Ветка: `stage-7.1.7-device-validation`.
- HEAD: `d7f33bc16e9d90bdffa1927fa68486d77b266c41`.
- `main` не изменялся.
- `git reset`, `clean`, `rebase`, force push не выполнялись.
- Commit/push не выполнялись.
- Все изменения Stage 15.2R/15.3/15.3.1 сохранены.
- Pre-existing изменения `docs/LUA_RUNTIME_RESEARCH.md` и
  `docs/STAGE_6_2_6_GITHUB_IOS_CORE_BUILD.md` не трогались.
- `.research-inputs/`/`research-inputs/` не изменялись и не добавлялись в Git.

## 2. Проверенная инфраструктура

Изучены:

- `CMakeLists.txt`, target `neo_loader_bootstrap`;
- `ios/bootstrap/src/bootstrap.cpp`;
- `ios/bootstrap/src/ios_frame_probe.mm`;
- `ios/bootstrap/include/neo_bootstrap.h`;
- `ios/bootstrap/include/neo_frame_probe.h`;
- `include/arizona_neo/runtime/frame_probe_state.hpp`;
- `tools/ipa_integration/integrate_ipa.py`;
- `tools/ipa_integration/audit_ipa.py`;
- `.github/workflows/ios-device-runtime.yml`;
- `.github/workflows/ios-integrated-ipa.yml`;
- Stage 13.4/15.3/15.3.1 reports.

Фактические IPA:

- оригинальная:
  `/root/ArizonaNeomios/inputs/Arizona-iOS-17.9.3-9281672.ipa`
  (61 829 555 bytes);
- предыдущая интегрированная:
  `/root/ArizonaNeo-research/stage-13.4/ArizonaNeo-integrated-unsigned.ipa`
  (62 066 964 bytes, SHA-256
  `de39850c79ee6f0658e01e6e01affb8aadb512dfe7ad86844a907141c56f42cb`).

Оригинальная IPA не добавлена в Git и не загружается workflow в публичные
artifacts.

## 3. Новый workflow

Создан:

`.github/workflows/ios-frame-probe.yml`

### 3.1 Build job

`build-frame-probe`:

- runner: `macos-15-intel` (тот же runner, что в существующем
  `ios-device-runtime.yml`);
- `workflow_dispatch`;
- permissions: `contents: read`, `actions: read`;
- Xcode/SDK определяются на runner:
  - `xcodebuild -version`;
  - `xcrun --sdk iphoneos --show-sdk-version/path`;
  - проверка `objc/runtime.h` и `Foundation.h`;
- pinned LuaJIT:
  - commit `96f7b297326e9fd454b7186c6858b2773f94a0dc`;
  - artifact ID `14855513382` (проверен через GitHub API:
    `expired=false`, expires `2027-02-21T19:50:50Z`, size 212 642 bytes);
  - ZIP SHA-256 `58ff22c6ebe8019ff0f11679e81df93c4a1052605daefdb9fae935d5d724f595`;
  - library SHA-256 `6b3c087831456891cf572f56aa9127b9539507b32c1552582eaf87c6deb31343`;
- CMake:

```bash
cmake -S . -B ci/build-ios \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_SYSROOT=iphoneos \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 \
  -DCMAKE_BUILD_TYPE=Release \
  -DARIZONA_NEO_WITH_LUAJIT=ON \
  -DLUAJIT_INCLUDE_DIR="$LUAJIT_INCLUDE_DIR" \
  -DLUAJIT_LIBRARY="$LUAJIT_LIB"
cmake --build ci/build-ios --target neo_loader_bootstrap --parallel 2
```

CMake уже включает `ios/bootstrap/src/ios_frame_probe.mm` в
`neo_loader_bootstrap` при `APPLE` (ветка добавлена в Stage 15.3.1), поэтому
используется существующая система сборки, а не отдельный Xcode-проект.

### 3.2 Проверки dylib в workflow

- `file`: Mach-O 64-bit, arm64;
- `lipo -info`: arm64;
- `otool -hv`: ARM64;
- `otool -l`: `platform 2` (iOS), `minos 15.0`;
- `otool -D`: `@rpath/libNeoLoaderBootstrap.dylib`;
- `otool -L`: только системные зависимости, без simulator/Android/Bionic/JNI;
- `nm -g`: bootstrap exports, включая `_neo_bootstrap_write_marker`;
- `strings`: `FRAME_PROBE_START`, `FRAME_PROBE_INSTALLED`,
  `FRAME_PROBE_INSTALL_FAILED`, `FRAME_TICK` (доказывает, что probe-код
  вошёл в dylib; скрытые C-символы не обязаны быть экспортированы);
- `__mod_init_func`/`__init_offsets` присутствует;
- SHA256 dylib.

## 4. Передача оригинальной IPA

GitHub Actions не имеет доступа к локальному файлу пользователя. Кроме того,
репозиторий `cocococecece0-stack/ArizonaNeo-iOS` — **публичный**. Поэтому
безопасного способа передать оригинальную Arizona IPA или опубликовать
интегрированную IPA через публичные Actions artifacts без отдельного решения
пользователя нет.

Итоговый workflow — **dylib-only**:

- artifact `ios-frame-probe-dylib`:
  - `libNeoLoaderBootstrap.dylib` (наш код, не оригинальная IPA);
  - `SHA256SUMS.txt`;
  - `toolchain-report.txt`, build/verification logs.

Оригинальная IPA не загружается в Actions и не добавляется в Git. Пользователь
скачивает dylib artifact и интегрирует IPA локально в Termux/Ubuntu:

```bash
python3 tools/ipa_integration/integrate_ipa.py \
  --ipa /root/ArizonaNeomios/inputs/Arizona-iOS-17.9.3-9281672.ipa \
  --dylib /path/to/libNeoLoaderBootstrap.dylib \
  --output /path/to/ArizonaNeo-FrameProbe-unsigned.ipa \
  --report /path/to/integration-report.json

python3 tools/ipa_integration/audit_ipa.py \
  --ipa /path/to/ArizonaNeo-FrameProbe-unsigned.ipa \
  --expected-dylib-sha256 <dylib-sha256> \
  --output /path/to/audit-report.json
```

`integrate_ipa.py`/`audit_ipa.py` — чистый Python, macOS не требуется.
Оригинальная IPA остаётся локальной; интегрированная IPA не публикуется в
публичных artifacts. Подпись — только через ESign пользователя.

## 5. Результат сборки в текущей среде

Workflow **не запускался**: изменения не закоммичены/не запушены, а GitHub
Actions выполняет только workflow из удалённой ветки. В локальной среде нет
macOS/Xcode/iPhoneOS SDK.

Поэтому:

- iOS dylib не собран;
- диагностическая IPA не создана;
- SHA256 dylib/IPA отсутствуют;
- Mach-O проверки из workflow не выполнялись.

Это не является ошибкой кода; это ограничение окружения и запрета на
commit/push.

## 6. Linux-тесты

Проверено последовательно:

- LuaJIT ON: **17/17 passed**;
- Lua 5.4 ON, LuaJIT OFF: **13/13 passed**;
- LuaJIT OFF, Lua 5.4 OFF: **12/12 passed**;
- `git diff --check`: без ошибок.

## 7. Ограничения

- Runtime trace не подтверждён.
- `GLView.tick` probe не проверялся на iPhone.
- Workflow не запускался без push.
- Оригинальная IPA не передаётся в Actions автоматически.
- Автоматическое подписание не добавлено; IPA остаётся unsigned для ESign.
- Workflow использует pinned LuaJIT artifact; при его недоступности job
  завершится ошибкой, а не подставит другой бинарник.

## 8. Точные дальнейшие действия

1. Закоммитить и запушить изменения (только с разрешения пользователя).
2. Открыть Actions → `iOS Frame Probe Dylib (unsigned, no IPA upload)` →
   `Run workflow`.
3. Скачать artifact `ios-frame-probe-dylib`, проверить `SHA256SUMS.txt` и
   `bootstrap-binary-info.txt`.
4. Локально интегрировать dylib в оригинальную IPA командами из раздела 4.
5. Подписать `ArizonaNeo-FrameProbe-unsigned.ipa` через ESign.
6. Установить, запустить Arizona, провести в игре 30+ секунд.
7. Получить `Library/Caches/neo_bootstrap.marker` и unified log
   (`FRAME_PROBE_*`, `FRAME_TICK`).
8. Проверять A–E раздельно, как в Stage 15.3.1. Не объявлять callback
   подтверждённым до появления runtime trace.

## 9. Вердикт

Stage 15.3.2 подготовил реальный macOS GitHub Actions workflow для сборки
arm64 iOS dylib с `GLView.tick` probe и безопасный путь локальной интеграции
IPA. Workflow не запускался из-за отсутствия commit/push и macOS-среды;
диагностическая IPA и runtime trace пока не получены.
