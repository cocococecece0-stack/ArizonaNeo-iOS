# Stage 15.3.1 — Device Frame Probe and Runtime Trace

Дата: 2026-11-26.

## 1. Preflight

- Ветка: `stage-7.1.7-device-validation`.
- HEAD: `d7f33bc16e9d90bdffa1927fa68486d77b266c41`.
- `main` не изменялся.
- `git reset`, `clean`, `rebase`, force push не выполнялись.
- Commit/push не выполнялись.
- Изменения Stage 15.2R и Stage 15.3 сохранены.
- Pre-existing изменения `docs/LUA_RUNTIME_RESEARCH.md` и
  `docs/STAGE_6_2_6_GITHUB_IOS_CORE_BUILD.md` не трогались.
- `.research-inputs/`/`research-inputs/` не изменялись и не добавлялись в Git.

## 2. Проверенная инфраструктура

Проверены:

- `ios/bootstrap/src/bootstrap.cpp` и `ios/bootstrap/include/neo_bootstrap.h`;
- constructor `neo_bootstrap_library_loaded`;
- `tools/ipa_integration/integrate_ipa.py` и `audit_ipa.py`;
- `LC_LOAD_DYLIB`/`LC_RPATH` injection;
- существующий diagnostic marker `Library/Caches/neo_bootstrap.marker`;
- `tests/ios_bootstrap_test.cpp`;
- Stage 13.x и Stage 15.3 reports.

Фактически доступные IPA (проверены заново, не предполагались):

- оригинальный IPA:
  `/root/ArizonaNeomios/inputs/Arizona-iOS-17.9.3-9281672.ipa`
  (61 829 555 bytes);
- предыдущий интегрированный IPA:
  `/root/ArizonaNeo-research/stage-13.4/ArizonaNeo-integrated-unsigned.ipa`
  (62 066 964 bytes, SHA-256
  `de39850c79ee6f0658e01e6e01affb8aadb512dfe7ad86844a907141c56f42cb`).

Новый диагностический IPA в этой среде не собран: нет macOS, Xcode и
iPhoneOS SDK. Поэтому путь/SHA256 итоговой IPA в отчёте не заявляются.

## 3. Реализованный probe

### 3.1 Portable state

Добавлен `include/arizona_neo/runtime/frame_probe_state.hpp`:

- атомарный счётчик тиков;
- rate-limit 1 раз в секунду;
- без аллокаций и файловых операций;
- thread-safe `should_log(now_ns, count)`, `count()`, `reset()`.

Добавлен тест `tests/frame_probe_state_test.cpp` и CMake target
`frame_probe_state_test`. Он проверяет первый лог, throttle внутри секунды,
границу секунды, reset и конкурентные вызовы из 4 потоков.

### 3.2 iOS-only Objective-C++ probe

Добавлен `ios/bootstrap/src/ios_frame_probe.mm` и
`ios/bootstrap/include/neo_frame_probe.h`.

Алгоритм:

1. `objc_getClass("_TtC7Arizona6GLView")`.
2. `sel_registerName("tick")`.
3. `class_getInstanceMethod`.
4. Проверка `method_getTypeEncoding(method) == "v16@0:8"`.
5. Проверка `method_getNumberOfArguments(method) == 2`.
6. `method_getImplementation` сохраняется как original IMP.
7. `method_setImplementation` на `neo_frame_probe_tick`.
8. Read-back `method_getImplementation`; при несовпадении original
   восстанавливается.
9. При гонке с другим hook (`previous != original`) изменение откатывается.
10. `neo_ios_frame_probe_is_installed()` и
    `neo_ios_frame_probe_tick_count()` доступны для диагностики.

Hardcoded `0x10001b0bc` **не используется** для поиска метода или установки.
Адрес из Stage 15.3 остаётся только доказательством для отчёта.

### 3.3 Сохранение оригинального поведения

`neo_frame_probe_tick` после диагностики вызывает сохранённый original IMP:

```objc
IMP original = g_original_tick.load(std::memory_order_acquire);
if (original != nullptr) {
    reinterpret_cast<void (*)(id, SEL)>(original)(self, command);
}
```

Оригинальный `tick` в исследованном бинарнике — `ret`; тем не менее вызов
сохранён, чтобы не менять поведение при другой сборке.

### 3.4 Безопасная установка

- `g_started` не допускает повторного запуска discovery.
- `g_installed` не допускает повторной установки.
- `g_failed` фиксирует окончательную ошибку.
- Если класс ещё не зарегистрирован, выполняется **bounded** retry на main
  queue: до 20 попыток по 100 ms (максимум ~2 секунды). Бесконечного polling
  и отдельных потоков нет.
- `GLView` ищется через `objc_getClass`, а не через hardcoded IMP.
- Установка выполняется на main queue; до проверки original IMP swizzle не
  ставится.

### 3.5 Логирование

Добавлен экспорт `neo_bootstrap_write_marker(event, detail)` в
`neo_bootstrap.h`/`bootstrap.cpp`. Он использует существующий marker
`Library/Caches/neo_bootstrap.marker` и `os_log`.

Markers:

- `FRAME_PROBE_START` — discovery запланирован;
- `FRAME_PROBE_INSTALLED` — swizzle установлен; detail содержит class,
  selector, type encoding, original/replacement pointers;
- `FRAME_PROBE_ALREADY_INSTALLED` — повторный вызов;
- `FRAME_PROBE_INSTALL_FAILED` — class/method/signature/read-back ошибка.

`FRAME_TICK` пишется через `os_log_info` не чаще одного раза в секунду:

```text
neo_frame_probe FRAME_TICK count=<N> timestamp_ns=<T>
```

В callback нет файловых операций, Lua, UIKit, NeoLoaderHost или `neo_tick`.
Файловый marker используется только для install/error событий вне callback.

### 3.6 Bootstrap lifecycle

`neo_bootstrap_library_loaded` вызывает
`neo_ios_frame_probe_schedule_start()`. Функция только ставит блок в main
queue; сам probe запускается позже, когда C++/ObjC static initialization уже
завершена. Это исключает гонку constructor со static initialization probe.

## 4. Сборка

### 4.1 Linux

Собраны и проверены все три конфигурации:

- LuaJIT ON: **17/17 passed**;
- Lua 5.4 ON, LuaJIT OFF: **13/13 passed**;
- LuaJIT OFF, Lua 5.4 OFF: **12/12 passed**.

Новый `frame_probe_state_test` проходит во всех конфигурациях.
`ios_bootstrap_test` проходит.

### 4.2 iOS

iOS-сборка **не выполнялась**: в окружении нет macOS/Xcode/iPhoneOS SDK.
Существующий workflow `.github/workflows/ios-device-runtime.yml` при запуске
на macOS соберёт `neo_loader_bootstrap` с `ios_frame_probe.mm` (CMake `APPLE`
ветка) и проверит Mach-O/platform/minos/exports.

Для статической проверки синтаксиса ObjC++ использован clang с минимальными
stub-заголовками (`objc/runtime.h`, `dispatch/dispatch.h`, `os/log.h`,
`Foundation/Foundation.h`):

```bash
clang++ -fsyntax-only -std=c++17 -fblocks -x objective-c++ \
  -I/tmp/opencode/stage153/stubs -Iinclude -Iios/bootstrap/include \
  ios/bootstrap/src/ios_frame_probe.mm
```

Проверка прошла без ошибок. Это **не** iOS build и не device validation.

### 4.3 IPA

Диагностическая IPA не создана. Причина: нет macOS/Xcode/SDK и невозможно
собрать корректный arm64 iOS dylib с Foundation/ObjC runtime. Существующий
Stage 13.4 IPA не содержит probe и не используется как доказательство.

## 5. Инструкция device-проверки

На macOS с Xcode:

1. Собрать `neo_loader_bootstrap` из текущего checkout с
   `ARIZONA_NEO_WITH_LUAJIT=ON`, `CMAKE_SYSTEM_NAME=iOS`,
   `CMAKE_OSX_ARCHITECTURES=arm64`, deployment target 15.0.
2. Проверить `file`/`lipo`/`otool`/`nm`:
   - Mach-O arm64, platform 2 (iOS), minos 15.0;
   - exports включают `neo_bootstrap_write_marker`;
   - нет Android/Bionic/JNI зависимостей.
3. Интегрировать dylib в оригинальный IPA:

```bash
python3 tools/ipa_integration/integrate_ipa.py \
  --ipa /path/to/Arizona-iOS-17.9.3-9281672.ipa \
  --dylib /path/to/libNeoLoaderBootstrap.dylib \
  --output /path/to/ArizonaNeo-frame-probe-unsigned.ipa \
  --report /path/to/integration-report.json
```

4. Audit:

```bash
python3 tools/ipa_integration/audit_ipa.py \
  --ipa /path/to/ArizonaNeo-frame-probe-unsigned.ipa \
  --expected-dylib-sha256 <dylib sha256> \
  --output /path/to/audit-report.json
```

5. Подписать IPA через ESign и установить.
6. Запустить Arizona, дождаться игрового интерфейса, провести в игре не менее
   30 секунд.
7. Получить marker:
   `Library/Caches/neo_bootstrap.marker` в контейнере приложения через
   ESign/Files.
8. Получить unified log через Console/`log collect` с фильтром
   `neo_frame_probe` или `FRAME_TICK`.

Ожидаемые marker-события:

```text
DYLIB_LOADED
FRAME_PROBE_START
FRAME_PROBE_INSTALLED detail=class=... selector=tick types=v16@0:8 ...
```

Ожидаемый unified log:

```text
neo_frame_probe FRAME_TICK count=1 timestamp_ns=...
neo_frame_probe FRAME_TICK count=2 timestamp_ns=...
```

`FRAME_TICK` в файле не пишется (это сделано намеренно, чтобы не выполнять
файловый I/O в render callback). Отсутствие `FRAME_TICK` в marker-файле не
является доказательством отсутствия callback; смотреть unified log.

## 6. Пять отдельных результатов

Не объединять:

| Результат | Что доказывает | Где смотреть |
|---|---|---|
| A. Dylib загружена | constructor выполнен | `DYLIB_LOADED` marker |
| B. GLView обнаружен | class найден | `FRAME_PROBE_INSTALLED` или `FRAME_PROBE_INSTALL_FAILED` с class=... |
| C. Swizzle установлен | method подменён и read-back успешен | `FRAME_PROBE_INSTALLED` marker |
| D. GLView.tick вызван | callback сработал | первый `FRAME_TICK` в unified log |
| E. Callback регулярный | count растёт, интервалы ~1 s | `FRAME_TICK` count/timestamp за 30+ секунд |

На текущий момент ни один из A–E не подтверждён на iPhone. Это требует
реального запуска.

## 7. Ограничения

- В callback нет Lua, `neo_tick`, `NeoLoaderHost::start`, SAMP callbacks,
  UIKit, файлового I/O и изменения рендера.
- Swizzle меняет только `GLView.tick`; оригинальный IMP вызывается.
- `_ui_frame_tick` не подменяется.
- Hardcoded IMP не используется для установки.
- Класс/метод проверяются по имени, type encoding и числу аргументов.
- Если класса/метода нет, установка не продолжается; пишется причина.
- Диагностический IPA не создавалась; iOS build не заявляется.

## 8. Следующий шаг

Если A–E подтверждены на устройстве:

1. Добавить безопасную очередь кадров и `NeoLoaderAPI::tick(delta)` вне
   render callback (drain на worker/main-loop).
2. Только после этого решать вопрос о вызове
   `neo_bootstrap_initialize()`/`neo_bootstrap_attach_host()`.
3. Для SAMP отдельно получить unstripped `libag-client` или runtime trace
   RPC handlers; `samp.events.dispatch` не подключать до подтверждения.

Если D/E не подтверждены — не использовать `GLView.tick` как frame source;
следующий кандидат — подтверждённый `_ui_frame_tick` interpose, но только
после отдельного анализа безопасности code patching.

## 9. Вердикт

Stage 15.3.1 реализовал диагностический probe и portable rate-limit state,
сохранил Stage 15.2R/15.3 и прошёл Linux-тесты. Runtime trace на iPhone пока
не получен; frame callback **не объявляется подтверждённым**.

## Stage 15.3.2 build pipeline

Для сборки probe на macOS добавлен workflow
`.github/workflows/ios-frame-probe.yml` и отчёт
`docs/STAGE_15_3_2_MACOS_BUILD_PIPELINE.md`. Workflow не запускался без
commit/push; диагностическая IPA пока не собрана.
