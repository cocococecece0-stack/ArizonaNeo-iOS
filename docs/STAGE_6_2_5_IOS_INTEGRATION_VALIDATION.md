# Stage 6.2.5 — iOS ARM64 LuaJIT Core Integration Validation

Дата: 2026-11-24. Хост проверки: Linux AArch64 (PRoot Ubuntu поверх Termux) — **не macOS**, поэтому iOS-линковка здесь не выполнялась и не может быть заявлена как успешная.

## 1. LuaJIT commit

`96f7b297326e9fd454b7186c6858b2773f94a0dc` — подтверждён:
- в env macOS-run (`LUAJIT_COMMIT` в логах run `45253149431`);
- version symbol в артефакте: `_luaJIT_version_2_1_1795119671` — совпадает с `luajit.h:35` локального дерева того же commit.

## 2. Artifact source (подтверждено)

- GitHub Actions run: https://github.com/cocococecece0-stack/ArizonaNeo-iOS/actions/runs/45253149431
- Workflow: `.github/workflows/luajit-ios.yml` (`macos-15-intel`, macOS 15.7.9, runner arch x86_64, cross → iOS arm64).
- Artifact: `luajit-ios-arm64`, id `14855513382`, 212642 байт (zip), upload 2026-11-23T19:51:41Z, не истёк.
- Artifact zip SHA256 из API/run-log: `58ff22c6ebe8019ff0f11679e81df93c4a1052605daefdb9fae935d5d724f595`.
- Свежая загрузка через `gh api repos/.../actions/artifacts/14855513382/zip` дала тот же zip; распакованный `libluajit.a` — 508008 байт, SHA256 `6b3c087831456891cf572f56aa9127b9539507b32c1552582eaf87c6deb31343`, совпадает с ранее сохранённой локальной копией.
- Linux `libluajit.a` (sha256 `963c7ea3…`) для iOS **не использовался**.

## 3. Apple toolchain

Локально (Linux): `xcodebuild`/`xcrun`/`otool`/`lipo` — **NOT FOUND**; `clang` — Termux `clang 21.1.8`, target `aarch64-unknown-linux-android24` (не Apple Clang). CMake 4.5.1. Попыток iOS-линковки на хосте не делалось.

Фактические версии с macOS runner (из логов run `45253149431`, только чтение):

| Компонент | Значение |
|---|---|
| macOS | 15.7.9 (24G830), image `macos-15` (20260824.0482.1) |
| Runner arch | x86_64 (кросс-компиляция в arm64) |
| Xcode | 16.4, build 16F6 |
| iPhoneOS SDK | 18.5, `/Applications/Xcode_16.4.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS18.5.sdk` |
| Apple Clang | 17.0.0 (clang-1700.0.13.5) |
| Apple linker | `ld` из Xcode 16.4 (использован через `clang -arch arm64 -isysroot ... -miphoneos-version-min=15.0`); нативный `ld` в логах не версионирован |
| CMake на macOS | В логах LuaJIT-run не фиксировался; отдельный `ios-core.yml` его тоже не печатает. Локальная версия 4.5.1 к macOS не относится |

## 4. Проверка iOS LuaJIT artifact (выполнено локально, LLVM-инструменты)

Использованы `llvm-nm`, `llvm-otool`, `llvm-objdump`, парсер Mach-O на Python (GNU `nm` не понимает Mach-O):

- 70 архивных членов, **все** `Mach-O 64-bit arm64 object` (`MH_MAGIC_64`, `cputype 0x0100000c = CPU_TYPE_ARM64`, `filetype MH_OBJECT`).
- **ELF-объектов: 0**; Android/iOS смешения нет (все 70 — iOS).
- `LC_BUILD_VERSION` у всех 70: `platform 2 = PLATFORM_IOS`, `minos 15.0.0` (SDK-поле у объектных файлов `n/a` — норма); флаг `-miphoneos-version-min=15.0` из workflow совпадает.
- `lipo` в CI-логе: `Non-fat file: ... is architecture: arm64`; `otool -hv`: `MH_MAGIC_64 ARM64`.
- JIT disabled: `LJ_OS_NOJIT=1` (iOS-платформа) и `XCFLAGS="-DLUAJIT_DISABLE_JIT"` в workflow → `LJ_HASJIT=0`; в архиве **0** символов `lj_trace_*`/`lj_record_*`/`lj_ir_*`.
- FFI enabled: `luaopen_ffi` определён (`T`); `LUAJIT_DISABLE_FFI` не задан; `LJ_ARCH_NOFFI` для ARM64 не определён. Runtime FFI на устройстве **не проверен**.
- Lua C API: `_luaL_newstate`, `_lua_pcall`, `_lua_close`, `_luaL_loadbuffer` и др. присутствуют (defined в архиве).

## 5. Undefined symbols analysis

- 636 defined / 452 undefined symbolов; **неразрешённых LuaJIT-внутренних (`_lua*`/`_lj*`) символов: 0** — замыкание архива полное.
- 85 внешних символов — только Apple system libc/libm/libSystem: `___error`, `___memcpy_chk`, `___stderrp`, `_acos…_tanh`, `_fopen/_fread/...`, `_sys_icache_invalidate` (Apple-специфичный), `_dlopen/_dlsym/_dlclose/_dlerror` (из `lj_clib.o` и `lib_package.o`; на iOS предоставляются libSystem, **отдельная libdl не нужна и не добавлялась**).
- **Отсутствуют**: glibc `__errno_location`, Android `__android_*`, JNI (`Java_*`, `JNIEnv`), Linux `pthread_*` как внешние зависимости, `libdl.so` как библиотека.
- Категорически не добавлялись фиктивные библиотеки для подавления ошибок линковщика.

## 6. iOS Core compile / static link — НЕ ВЫПОЛНЕНО (blocker)

- На хосте нет Xcode/iPhoneOS SDK; согласно заданию iOS-линковка на Linux AArch64 не выполнялась.
- Существующий `.github/workflows/ios-core.yml` собирает `libarizona_neo_core.a` для iOS arm64 **без `ARIZONA_NEO_WITH_LUAJIT`** — текущая iOS CI не проверяет LuaJIT-интеграцию.
- Подготовлен `scripts/validate_ios_luajit_core.sh` (macOS-only, guard `uname -s == Darwin`): скачивает закреплённый artifact с проверкой SHA256, берёт headers того же commit, конфигурирует отдельный CMake build с `ARIZONA_NEO_WITH_LUAJIT=ON`, собирает Core, проверяет `file`/`lipo`/`otool`/`nm`, отдельно запускает host-тесты. **Скрипт подготовлен, но не запускался** (нет macOS runner). Добавлен в `.gitignore` каталог `.ios-luajit-validation/`.
- Конечный Mach-O ARM64 бинарник Core+LuaJIT в этом этапе **не получен**, поэтому `file`/`lipo -info`/`otool -hv`/`otool -L`/`nm -u`/`nm -g` для него не предъявлены (проверены только исходный LuaJIT-артефакт).

## 7. Runtime smoke test на устройстве — НЕ ВЫПОЛНЕНО

iOS-устройство/симулятор недоступны. Сценарии `initialize/execute/arithmetic/string/runtime error/FFI struct/shutdown/re-init` на iOS **не запускались**; успех не имитируется. Переносить результаты Linux Debug/Release (Stage 6.2.4) на iOS нельзя.

## 8. Интеграция с приложением (анализ без изменения артефактов)

- В репозитории **нет** `.xcodeproj`, `Info.plist`, bundle ID, entitlement или IPA — проверить конфликт символов с игровым executable невозможно.
- Точка подключения по текущей архитектуре: `CMakeLists.txt` при `ARIZONA_NEO_WITH_LUAJIT=ON` добавляет `src/runtime/lua_jit_host.cpp` в `arizona_neo_core` и `target_link_libraries(... PUBLIC ${LUAJIT_LIBRARY} m)`. Приложение получит LuaJIT транзитивно через `libarizona_neo_core.a`; явного изменения игрового executable или bundle ID не требуется.
- `:m` — единственная добавленная системная библиотека; на iOS `libm` входит в libSystem, `-lm` допустим. `extern "C"`-обёртка в `lua_jit_host.cpp` присутствует (исправление Stage 6.2.3); Linux-only API и Bionic в host-коде отсутствуют (проверено Stage 6.2.4); C++17 без расширений.
- Минимальный deployment target артефакта (15.0) совпадает с требованиями проекта (iOS 15+) и `ios-core.yml` (`CMAKE_OSX_DEPLOYMENT_TARGET=15.0`).
- Риск, который нельзя исключить статически: если игровое приложение уже линкует Lua/LuaJIT, возможны duplicate symbols. Требуется проверка при появлении исходников/артефакта приложения. LuaJIT **не внедрялся** ни в какой IPA автоматически.

## 9. Итог и blockers

Подтверждено: подлинный iOS ARM64 артефакт с нужного run'а, Mach-O ARM64, platform iOS, minos 15.0, JIT off, FFI on, полное замыкание Lua C API, отсутствие ELF/Bionic/JNI-зависимостей.

Не подтверждено (blockers): реальная iOS compile/link Core+LuaJIT на macOS runner; бинарные проверки `file/lipo/otool/nm` для Core+LuaJIT; runtime smoke test на устройстве; анализ конфликтов символов с игровым приложением.

Критерий завершения Stage 6.2.5 (успешная компиляция и линковка Core с LuaJIT iOS ARM64) **не достигнут в этой среде**. К Stage 6.3 не переходим. Следующий шаг — запустить подготовленный скрипт на `macos-15` runner (потребуется отдельное разрешение на изменение CI/отправку изменений; в этом этапе commit/push не выполнялись).
