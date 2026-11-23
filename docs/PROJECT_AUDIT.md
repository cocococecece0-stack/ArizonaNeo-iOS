# Аудит проекта ArizonaNeo-iOS

Дата аудита: 2026-11-23 (обновлено при Stage 4.1 — верификация и восстановление исследовательской документации).
Репозиторий: `https://github.com/cocococecece0-stack/ArizonaNeo-iOS`
Ветка: `main`
Последний коммит: `e4a8daa Fix iOS build runner: use macOS 15`
Предыдущий коммит: `7c65d9a Initial ArizonaNeo iOS research scaffold`
Статус Git: рабочее дерево содержит незакоммиченные изменения — восстановленные Stage 4 документы (untracked) и обновлённые `docs/PORTING_PLAN.md`, `docs/PROJECT_AUDIT.md`. Commit/push не выполнялись." oldString">
Дата аудита: 2026-11-23 (текущая дата среды разработки).
Репозиторий: `https://github.com/cocococecece0-stack/ArizonaNeo-iOS`
Ветка: `main`
Последний коммит: `e4a8daa Fix iOS build runner: use macOS 15`
Предыдущий коммит: `7c65d9a Initial ArizonaNeo iOS research scaffold`
Статус Git: чистый (без незакоммиченных изменений), ветка актуальна относительно `origin/main`.

## 0. Связанные документы

- `docs/PORTING_PLAN.md` — план этапов 0–7.
- `docs/RESEARCH_NOTES.md` — исследовательские заметки.
- `docs/ABI_RESEARCH.md` — исследование ABI/Mach-O/ELF и совместимости (Stage 3).
- `docs/NEOMLOADER_API_INVENTORY.md` — инвентарь API NeoLoader (Stage 4).
- `docs/IOS_COMPATIBILITY_MATRIX.md` — матрица совместимости Android→iOS.
- `docs/IOS_RUNTIME_ARCHITECTURE.md` — архитектура iOS runtime.
- `docs/SCRIPT_COMPATIBILITY_LEVELS.md` — уровни совместимости скриптов.
- `docs/CORE_INFRASTRUCTURE.md` — инфраструктура Core (Stage 5).
- `docs/STAGE_4_REPORT.md` — отчёт Stage 4.

## 1. Назначение проекта

`ArizonaNeo-iOS` — исследовательский порт NeoMLoader (Android-версия Arizona Role Play) на iOS.
Целевая платформа: iOS 15+, архитектура ARM64. Установка через ESign (планируется отдельно, не на этом этапе).
NeoMLoader является целевым загрузчиком. MonetLoader рассматривается только как архитектурный референс, а не замена.

## 2. Структура проекта

```
ArizonaNeo-iOS/
├── .github/
│   └── workflows/
│       └── ios-core.yml          # CI: сборка C++ Core под iOS ARM64
├── .gitignore
├── CMakeLists.txt                # сборка статической библиотеки arizona_neo_core
├── README.md
├── AGENTS.md                     # постоянные правила проекта (создан в этом аудите)
├── docs/
│   ├── PORTING_PLAN.md           # план этапов 0–7
│   ├── RESEARCH_NOTES.md         # исследовательские заметки
│   └── PROJECT_AUDIT.md          # этот файл
├── include/
│   └── arizona_neo/
│       └── core/
│           ├── lua_host.hpp      # интерфейс Lua-хоста (абстрактный)
│           └── version.hpp       # функция version()
└── src/
    └── core/
        ├── lua_host.cpp         # пустая заглушка (без реализации)
        └── version.cpp          # реализация version()
```

Каталоги `platform/ios/`, `compatibility/`, упомянутые в README, **отсутствуют**. Каталог `tests/` присутствует (1 тест: `version_test.cpp`).

## 3. Состояние основных компонентов

| Компонент | Файл(ы) | Состояние |
|---|---|---|
| Сборочная система | `CMakeLists.txt` | Подключён CTest (`include(CTest)`), тестовый подкаталог `tests/`. Статическая библиотека `arizona_neo_core` (2 cpp-файла). Версия проекта 0.1.0. C++20. iOS-специфичные параметры задаются в CI. |
| Версия | `include/.../version.hpp`, `src/core/version.cpp` | Реализовано: `version()` возвращает `"0.1.0-research"`. Покрыто тестом `tests/version_test.cpp`. |
| Интерфейс Lua-хоста | `include/.../lua_host.hpp` | Абстрактный класс `LuaHost`: `initialize()`, `execute(source, chunk_name)`, `shutdown()`. Реализации нет. Аудит API — раздел 5.1. |
| Реализация Lua-хоста | `src/core/lua_host.cpp` | Заглушка: только комментарий. Lua runtime не подключён. |
| CI | `.github/workflows/ios-core.yml` | `macos-15`: сборка iOS (`iphoneos`, `arm64`, deployment target 15.0), проверка артефакта, отдельная host-сборка и запуск `ctest`. |
| Документация | `README.md`, `docs/PORTING_PLAN.md`, `docs/RESEARCH_NOTES.md`, `docs/PROJECT_AUDIT.md` | Актуализированы для Stage 2. |
| Тесты | `tests/version_test.cpp`, `tests/CMakeLists.txt` | 1 тест: непустая версия, соответствие `"0.1.0-research"`, доступность через публичный заголовок. Проходит в Debug и Release. |
| Лицензии | — | Файл лицензии отсутствует. Сторонние компоненты не подключены. |

## 4. Результаты проверки сборки

### 4.1. Локальная сборка (Ubuntu/Termux, host aarch64)

Использованные команды:

```bash
rm -rf build/local
cmake -S . -B build/local \
  -DCMAKE_SYSTEM_NAME=Linux \
  -DCMAKE_SYSTEM_VERSION=6.8 \
  -DCMAKE_C_COMPILER=/usr/bin/gcc \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++ \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build/local --parallel
```

Результат: **успешно**. Создана статическая библиотека `build/local/libarizona_neo_core.a`, содержащая символ `arizona_neo::core::version()`; `lua_host.cpp.o` не экспортирует реализацию (файл пуст).

Дополнительно проверена компиляция обоих исходников отдельно компилятором Clang:

```bash
clang++ -std=c++20 -I include -c src/core/version.cpp -o /tmp/opencode/version.o
clang++ -std=c++20 -I include -c src/core/lua_host.cpp -o /tmp/opencode/lua_host.o
```

Результат: **успешно**, ошибок компиляции нет.

Примечание по среде: запуск происходил в Termux на Android-хосте, где CMake по умолчанию определяет host-систему как Android и пытается читать `$PREFIX/include/android/api-level.h`. Чтобы выполнить host-сборку под Linux, потребовалось явно задать `CMAKE_SYSTEM_NAME=Linux` и `CMAKE_SYSTEM_VERSION`. Это ограничение среды разработки, а не дефект CMakeLists.txt. В CI на macOS таких проблем нет.

### 4.2. Сборка под iOS

Локально невозможна: в окружении нет Xcode и iOS SDK (`iphoneos` sysroot). Проверка iOS-сборки делегирована GitHub Actions (`.github/workflows/ios-core.yml`, runner `macos-15`). Конфигурация workflow выглядит корректной для сборки статической библиотеки под iOS ARM64 с deployment target 15.0. Фактический прогон workflow в рамках этого аудита не выполнялся (нет доступа к GitHub Actions из локальной среды).

## 5. Обнаруженные проблемы

### 5.1. Аудит API LuaHost (Stage 2)

Интерфейс `LuaHost` не зависит от Android/JNI/ELF и пригоден как база для будущей реализации LuaJIT на iOS. Обнаруженные замечания (не исправлялись на этом этапе, чтобы не менять архитектуру без необходимости):

1. `execute()` возвращает только `bool` без канала диагностики/сообщения об ошибке Lua. Будущей реализации потребуется reporting ошибок (метод или callback).
2. `std::string_view` для исходного кода не гарантирует null-терминатор; реализация LuaJIT должна корректно обрабатывать длину (копировать в `std::string` или использовать `luaL_loadbuffer`).
3. Класс имеет неявные операции копирования/перемещения; для полиморфного интерфейса в будущем стоит рассмотреть удаление copy/move (`= delete`) во избежание срезки.
4. `initialize()` не помечен `noexcept` (в отличие от `shutdown()`). Это не ошибка, но стоит выровнять контракт.

Реализация `LuaHost` по-прежнему отсутствует.

1. **Отсутствуют тесты.** В репозитории нет ни одного теста и нет тестового таргета в CMake. Проверка сборки ограничена компиляцией.
2. **Lua runtime не реализован.** `LuaHost` — только абстрактный интерфейс; `lua_host.cpp` пуст. Это осознанный этап 0, но дальнейшая работа не может начинаться без исследования ABI и выбора LuaJIT/альтернативы.
3. **Нет iOS-адаптеров и каталогов**, заявленных в README (`platform/ios/`, `compatibility/`). Архитектура описана, но не создана.
4. **CMakeLists.txt не содержит iOS-специфических настроек** (deployment target, архитектура, sysroot). Они передаются только через командную строку в CI. Локальная сборка по умолчанию не воспроизводит iOS-конфигурацию.
5. **Нет файла лицензии** и нет политики проверки лицензий сторонних компонентов.
6. **Нет обработки ошибок/логирования** в интерфейсах. `LuaHost::execute` возвращает `bool` без диагностики.
7. **README упоминает «Produce a separately signed test IPA»**, но на этом этапе сборка/подпись IPA запрещена и не реализована.
8. **Внешние Android-данные** (скрипты NeoMLoader/MonetLoader, `.so`-библиотеки) присутствуют в общей папке Termux/Android, но не являются частью репозитория и не должны копироваться напрямую. Их можно использовать только как исследовательский референс.
9. **Нет проверки ABI/экспортированных символов** оригинального iOS-бинарника Arizona (`Frameworks/libag-client.dylib`) — это отдельный этап исследования.

## 6. Необходимые зависимости

Текущие (для сборки C++ Core):
- CMake ≥ 3.24
- Компилятор C/C++ с поддержкой C++20 (GCC или Clang)
- Для iOS-сборки: macOS с Xcode, iOS SDK (`iphoneos`), `cmake` с генератором, поддерживающим iOS toolchain

Планируемые (не подключены, требуют проверки лицензий):
- Lua runtime (LuaJIT или совместимая альтернатива) для iOS ARM64
- Инструменты для анализа Mach-O/экспортированных символов
- Тестовый фреймворк (CMake/CTest или отдельный) — выбор отложен

Не допускается:
- Прямое копирование Android ELF (`.so`) в iOS-проект
- Подключение непроверенных/сомнительных библиотек
- Фиктивная реализация Lua runtime

## 7. Рекомендуемый порядок следующих этапов

1. **Этап 1 — инвентаризация зависимостей NeoMLoader.** Собрать список используемых API/символов, скриптов, библиотек Android-версии (без копирования бинарников).
2. **Этап 2 — исследование iOS Mach-O.** Проверить `Arizona` и `Frameworks/libag-client.dylib`: архитектура, экспорты, совместимость с iOS 15+.
3. **Этап 3 — выбор и интеграция Lua runtime для iOS ARM64.** Только после проверки лицензий и ABI.
4. **Этап 4 — файловая система, планировщик, менеджер скриптов, логирование.** Создать каталоги `platform/ios/`, `compatibility/`, добавить тесты.
5. **Этап 5 — iOS game bridge.**
6. **Этап 6 — совместимость с существующими скриптами NeoMLoader.**
7. **Этап 7 — отдельная упаковка/подпись IPA через ESign** (только после явного разрешения).

Каждый этап должен сопровождаться отчётом и запуском подходящих тестов перед переходом к следующему.

## 8. Что сделано в этом аудите

- Изучены все файлы репозитория.
- Проверены `CMakeLists.txt`, исходники, заголовки, CI, документация.
- Проверен Git-статус и история (ветка `main`, 2 коммита, чистое дерево).
- Определено, что реализовано (`version()`), а что является заглушкой (`lua_host.cpp`).
- Найдены технические проблемы (раздел 5).
- Проверена локальная сборка C++ Core (успешно, раздел 4.1).
- Созданы `docs/PROJECT_AUDIT.md` и `AGENTS.md`.
- Архитектура не изменена. Код Lua runtime не добавлялся. Бинарники не модифицировались. IPA не собирался/не подписывался.
- Внешние Android-библиотеки не переносились.

## 10. Stage 4.1 — верификация и восстановление исследовательской документации

- Проверены файлы проекта и Git-история.
- Проверена доступность `/root/ArizonaNeo-research`: изначально пуста; восстановлена из `/mnt/sdcard/Android/media/com.arizona21.game.web/neomloader/` (300 файлов, 210 `.lua`, 3 `.so`).
- Повторён статический анализ реальных Lua-модулей.
- Восстановлены 6 документов Stage 4: `NEOMLOADER_API_INVENTORY.md`, `IOS_COMPATIBILITY_MATRIX.md`, `IOS_RUNTIME_ARCHITECTURE.md`, `SCRIPT_COMPATIBILITY_LEVELS.md`, `NATIVE_LIBRARY_ANALYSIS.md`, `STAGE_4_REPORT.md`.
- Обновлены `PORTING_PLAN.md` и `PROJECT_AUDIT.md`.
- C++-реализация не изменялась. Commit/push не выполнялись. Тесты проходят (1/1).

- Добавить тесты (хотя бы smoke-тест для `version()`).
- Реализовать iOS-адаптеры и каталоги, заявленные в README.
- Провести исследование ABI/экспортов оригинального iOS-бинарника.
- Выбрать и легально интегрировать Lua runtime для iOS ARM64.
- Настроить локальную воспроизводимую сборку под iOS (через macOS/CI).
- Добавить файл лицензии и политику проверки лицензий зависимостей.
