# ABI Research — ArizonaNeo-iOS (Stage 3)

Дата: 2026-11-23.
Цель: определить реальный путь переноса NeoMLoader на iOS 15+/ARM64.
Исследовательский этап: оригинальные бинарники не изменялись, загрузчик не реализовывался, IPA не собирался.

## 1. Инвентаризация доступных материалов

### 1.1. Найденные материалы

| Материал | Путь / источник | Статус |
|---|---|---|
| Репозиторий ArizonaNeo-iOS | `/root/ArizonaNeo-iOS` | Доступен |
| Установленное приложение Arizona (Android) | пакет `com.arizona21.game.web` | Установлено, данные в shared storage |
| Установленное приложение Arizona (Android) | пакет `com.arizona.game` | Установлено, данные в shared storage |
| NeoLoader-данные | `/mnt/sdcard/Android/media/com.arizona21.game.web/neomloader/` | Доступны (скрипты, ресурсы, 3 `.so`) |
| MonetLoader-данные | `/mnt/sdcard/Android/media/com.arizona.game/monetloader/` | Доступны (только как референс) |
| MoonMonet native lib | `.../neomloader/lib/MoonMonet/libmoonmonet*.so` | Доступны (3 ELF) |

### 1.2. Отсутствующие материалы (не найдены локально)

- iOS IPA Arizona RP (не найден).
- Android APK Arizona RP (не найден в `Download`/`Documents`/проекте; пакет установлен, но APK-файл недоступен).
- ZIP `arz_lua_launcher` (не найден).
- ZIP MonetLoaderOSS (не найден).
- Оригинальные `libneomloader.so` и `libag-client.so` (не найдены в доступных каталогах).
- Оригинальный iOS `Frameworks/libag-client.dylib` (нет IPA).

Вывод: полный бинарный анализ NeoMLoader и iOS-компонента **невозможен** на данном этапе. Результаты ниже ограничены доступными скриптами, ресурсами и MoonMonet ELF. Отсутствие материалов зафиксировано, а не скрыто.

### 1.3. Правило обращения с материалами
Оригинальные IPA/APK/ELF/Mach-O, скрипты и приватные данные **не добавлялись** в Git и не модифицировались.

## 2. Использованные инструменты

| Инструмент | Наличие | Применение |
|---|---|---|
| `file` | есть | тип файла, архитектура ELF |
| `strings` | есть | поиск строк (не использовался как источник «экспорта») |
| `readelf` | есть | ELF-заголовки, динамические зависимости, динамические символы |
| `llvm-nm` / `llvm-objdump` / `llvm-otool` | есть | доступны для Mach-O/ELF, но без IPA/APK не применены к оригинальным бинарникам |
| `unzip`, `jar` | есть | не применены (нет архивов) |

## 3. Исследование iOS Mach-O

**Статус: не выполнено — исходный IPA недоступен.**

Без IPA невозможно:
- определить структуру bundle;
- найти основной executable и `Frameworks/libag-client.dylib`;
- проверить архитектуры, deployment target, load commands;
- получить реальные imports/exports;
- проверить зависимости от системных frameworks;
- подтвердить строки, связанные с Lua/loader/hooks.

Вывод: любые утверждения об экспортах iOS-бинарника на данном этапе были бы выдумкой. Они не делаются.

## 4. Исследование Android ELF (доступный фрагмент)

### 4.1. Оригинальные библиотеки

`libneomloader.so` и `libag-client.so` **не найдены**. Их ELF-анализ (архитектура, динамические символы, зависимости, JNI-символы) **не выполнен**.

### 4.2. Доступный ELF: MoonMonet

Файлы:
- `lib/MoonMonet/libmoonmonet.so` — 505 624 байт
- `lib/MoonMonet/libmoonmonet_rs_arm64.so` — 505 624 байт
- `lib/MoonMonet/libmoonmonet_rs_arm.so` — 118 968 байт

Результаты `file`:
- `libmoonmonet.so`: ELF 64-bit LSB shared object, **ARM aarch64**, Android 26, NDK r28b, not stripped.
- `libmoonmonet_rs_arm64.so`: ELF 64-bit LSB shared object, **ARM aarch64**, Android 26, NDK r28b, not stripped.
- `libmoonmonet_rs_arm.so`: ELF 32-bit LSB shared object, **ARM**, Android 26, NDK r25b, stripped.

Результаты `readelf` для `libmoonmonet_rs_arm64.so`:
- Class: ELF64
- Type: DYN
- Machine: AArch64
- NEEDED: `libdl.so`, `libm.so`, `libc.so`
- Динамические символы: 50 записей (в основном импорты libc/libm и служебные символы; реальные экспорты MoonMonet в этой выборке не выявлены как стабильный API).

Вывод: это Android ELF, собранный под Bionic/Android API 26. **Прямое использование на iOS невозможно**. Библиотека может служить только как пример нативного компонента NeoMLoader-экосистемы и источник строк/символов для дальнейшего исследования.

## 5. Инвентарь Lua-ресурсов NeoLoader (Android)

Источник: `/mnt/sdcard/Android/media/com.arizona21.game.web/neomloader/`.
Всего файлов: 300, из них `.lua`: 210, `.so`: 3.

### 5.1. Верхнеуровневые библиотеки (`lib/`)

| Файл | Размер (байт) | Назначение (по содержимому/имени, не подтверждено) |
|---|---:|---|
| `moonloader.lua` | 3008 | Константы/таблицы MoonLoader (TAG, download_status, font flags, D3D примитивы, audiostream) |
| `monetloader.lua` | 221 | Константы MonetLoader (touch_events) |
| `memory.lua` | 7520 | FFI-обёртка над `mprotect`/`memcmp`, чтение/запись памяти, защита страниц |
| `imgui.lua` | 38 | Re-export `mimgui.compat_imgui` |
| `sampfuncs.lua` | 14501 | SAMP-функции (по имени) |
| `RakLua.lua` | 780 | RakNet Lua (по имени) |
| `samp/` | — | SAMP events, raknet, synchronization, bitstream |
| `SAMemory/` | — | Описания игровых структур (CPed, CVehicle, CCamera и др.) |
| `game/` | — | `weapons.lua`, `globals.lua`, `models.lua`, `keys.lua` |
| `mimgui/` | — | ImGui Lua-биндинги, шрифты, cdefs/enums |
| `socket/`, `ssl/`, `https.lua` | — | Сетевой стек LuaSocket/LuaSec |
| `copas/`, `copas.lua` | — | Асинхронное сетевое программирование |
| `cjson/`, `dkjson.lua`, `jsoncfg.lua` | — | JSON |
| `base64.lua`, `basexx.lua`, `md5.lua`, `sha1/`, `sha256.lua` | — | Кодирование/хэши |
| `encoding.lua`, `ltn12.lua`, `mime.lua` | — | Кодировки/потоки |
| `timerwheel.lua`, `binaryheap.lua` | — | Планировщик/таймеры |
| `requests.lua`, `snet/`, `snet.lua` | — | HTTP/сеть |
| `webviews/` | — | Веб-представления (по имени) |
| `MoonMonet/` | — | Нативная библиотека + `init.lua` |
| `android/` | — | `arizona.lua`, `jni-raw.lua`, `jnienv.lua`, `jnienv-util.lua`, `jar/arzapi.jar` |
| `jit/` | — | LuaJIT bytecode/disassembler (`dis_arm.lua`, `dis_arm64.lua` и др.) |

### 5.2. Android-специфичные компоненты

- `lib/android/arizona.lua` (1427 байт), `lib/android/jni-raw.lua`, `lib/android/jnienv.lua` (64927), `lib/android/jnienv-util.lua` (14759), `lib/android/jar/arzapi.jar` (1762).
- Эти файлы **напрямую зависят от JNI/Android runtime**. На iOS они не могут использоваться без полной переработки. Их функциональность не подтверждена анализом; известно только наличие JNI-ориентированного кода.

### 5.3. Игровой контекст

- `SAMemory/game/*` описывает структуры GTA SA (CPed, CVehicle, CCamera и т.д.) через FFI/offsets.
- `game/globals.lua`, `models.lua`, `weapons.lua`, `keys.lua`.
- Это указывает на сильную зависимость от внутреннего API/памяти GTA SA Android. На iOS игра имеет другую архитектуру (Mach-O, dylib), поэтому прямой перенос offsets/хуков не гарантирован.

## 6. Таблица совместимости компонентов

| Компонент | Назначение | Android-зависимости | Возможность iOS-порта |
|---|---|---|---|
| Lua runtime | Выполнение скриптов | LuaJIT/Android (по ресурсам `jit/`) | Требуется отдельный LuaJIT/альтернатива под iOS ARM64; не реализовано |
| Memory API | Чтение/запись памяти | `mprotect`, FFI, offsets GTA SA | Частично: POSIX `mprotect` доступен, но offsets/защита памяти iOS отличаются; без IPA не подтверждено |
| ImGui | Интерфейс | `mimgui`, шрифты, cdefs | Возможен отдельный ImGui для iOS/Metal, но нужна проверка лицензий и рендерера |
| Game API | Взаимодействие с игрой | Внутренние структуры GTA SA, SAMP | Высокий риск; зависит от iOS-бинарника, не подтверждено |
| Hooks | Перехват функций | Внутренние адреса/символы | На iOS без jailbreak сильно ограничено; не обещается |
| Socket/SSL | Сеть | LuaSocket/LuaSec | Высокая вероятность переноса (POSIX/BSD sockets), но нужна проверка зависимостей и лицензий |
| JNI bridge | Android API | JNI/Bionic | **Не переносится** на iOS в текущем виде |
| SAMemory offsets | Память GTA SA | Android offsets | Не подтверждено для iOS; требует отдельного исследования |

## 7. Ограничения iOS (без jailbreak)

- Обычное подписанное IPA не может внедрять код в произвольные процессы.
- Доступ к памяти другой процесса/игры недоступен без специальных прав.
- Хуки/инъекция библиотек в игру не гарантированы и, как правило, недоступны без jailbreak или специальных механизмов.
- Mach-O/DYLD, sandbox и code signing накладывают ограничения на динамическую загрузку кода.
- LuaJIT под iOS ARM64 имеет отдельные требования (JIT-права, подпись, ограничения), которые нужно проверить отдельно.
- Вывод: на iOS реалистичен **standalone-компонент** (Lua runtime, файлы, логи, сеть, UI) и, возможно, интеграция только при наличии официальной/допустимой точки входа в игру. Внедрение в произвольные процессы не обещается.

## 8. Что можно перенести / адаптировать / написать заново

### Можно повторно использовать (с проверкой лицензий)
- Чистые Lua-модули без Android-зависимостей: JSON, base64, socket (частично), timerwheel, binaryheap, memory-логика на уровне алгоритмов.
- Структуры данных и константы MoonLoader/MonetLoader.

### Требует адаптации
- Пути файловой системы (Android → iOS sandbox).
- Сетевой стек (проверка зависимостей).
- ImGui (рендерер/ввод под iOS).
- Планировщик и логирование.

### Нужно написать заново
- iOS Mach-O loader bridge (если вообще возможен и разрешён).
- Lua runtime интеграция под iOS ARM64.
- Game bridge под iOS.
- Система тестов.

### Невозможно определить без исходников/бинарников
- Реальные экспорты `libneomloader.so` и `libag-client.dylib`.
- Внутренние адреса/символы игры на iOS.
- Совместимость offsets GTA SA.
- Требования JIT на целевом устройстве.

## 9. Неизвестные вопросы

1. Где физически находятся `libneomloader.so` и `libag-client.so` в доступной среде? (не найдены)
2. Доступен ли IPA Arizona RP? (нет)
3. Какие символы реально экспортирует NeoMLoader? (неизвестно)
4. Какая точка входа используется для загрузки на Android? (не подтверждено)
5. Какие функции игры доступны на iOS? (не подтверждено)
6. Какие лицензии у LuaJIT/ImGui/других зависимостей? (не проверены)
7. Поддерживается ли JIT на целевой iOS-конфигурации? (не проверено)

## 10. Рекомендации для Stage 4

1. Получить и локально (без добавления в Git) исследовать iOS IPA и Android APK Arizona RP; повторить Mach-O/ELF анализ с реальными бинарниками.
2. Построить инвентарь экспортируемых символов NeoMLoader и сравнить с iOS.
3. Не переносить `android/` JNI-слой; спроектировать нейтральный интерфейс.
4. Начать с файловой системы, логирования, планировщика и конфигов (переносимая часть).
5. Отдельно проверить лицензии LuaJIT и ImGui.
6. Расширить тесты: проверки путей/конфигов без реального runtime.
7. Обновлять `docs/PROJECT_AUDIT.md` и `docs/ABI_RESEARCH.md` по мере появления реальных данных.
