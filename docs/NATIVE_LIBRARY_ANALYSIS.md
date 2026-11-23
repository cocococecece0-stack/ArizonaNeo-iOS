# Native Library Analysis (Stage 4)

Анализ нативных библиотек (ELF/JAR) из установленного NeoLoader 1.0.72.
Бинарники не запускались; использованы `file`, `readelf`, `nm`/`llvm-nm` (только метаданные/символы). Оригинальные бинарники не изменялись.

## 1. Доступные нативные артефакты

| Артефакт | Путь (относительно neomloader/) | Размер | Тип |
|---|---|---:|---|
| `libmoonmonet.so` | `lib/MoonMonet/libmoonmonet.so` | 505 624 | ELF 64-bit LSB shared, ARM aarch64, Android 26, NDK r28b, not stripped |
| `libmoonmonet_rs_arm64.so` | `lib/MoonMonet/libmoonmonet_rs_arm64.so` | 505 624 | ELF 64-bit LSB shared, ARM aarch64, Android 26, NDK r28b, not stripped |
| `libmoonmonet_rs_arm.so` | `lib/MoonMonet/libmoonmonet_rs_arm.so` | 118 968 | ELF 32-bit LSB shared, ARM, Android 26, NDK r25b, stripped |
| `arzapi.jar` | `lib/android/jar/arzapi.jar` | 1 762 | APK/DEX (classes.dex, 2 828 байт) |
| `WebViews.jar` | `lib/webviews/WebViews.jar` | 18 778 | APK/DEX (classes.dex, 51 488 байт) + MANIFEST.MF |

Отсутствуют (не найдены в доступной среде):
- `libneomloader.so`
- `libag-client.so` / `libag-client.dylib`
- `RakLuaSO` (нативный модуль, упоминается в `RakLua.lua`)
- `NeoSAMemory` (нативный модуль, упоминается в `SAMemory/init.lua`)
- `effil` (потоки)
- `mime.core`, `cjson` (нативные Lua-модули)
- `bass` (аудио, загружается через `NeoMLoader_GetBassSymbol`)

## 2. MoonMonet (ELF)

### 2.1. Общие свойства

- Формат: ELF, динамически связанная shared library.
- Целевая ОС: Android 26 (Bionic).
- Архитектуры: ARM64 (aarch64) и ARM (32-bit).
- Зависимости (NEEDED): `libdl.so`, `libm.so`, `libc.so` (только стандартные Android-библиотеки).
- Экспорты (динамические символы типа `T`): только `moonmonet_build_colors`.

### 2.2. Экспортируемый API

Сигнатура (из `lib/MoonMonet/init.lua` и cdef):
```c
Colors moonmonet_build_colors(float chroma_multiplier, uint32_t color, bool accurate_shades);
```
Структуры:
```c
typedef struct { uint32_t color_0..color_1000; } SubColors; // 13 полей
typedef struct { SubColors accent1, accent2, accent3, neutral1, neutral2; } Colors;
```

### 2.3. Совместимость с iOS

- ELF Android не может быть загружен на iOS (Mach-O, dyld, code signing).
- Библиотека зависит только от libc/libm/libdl; логика цветовой палитры, вероятно, портируема, но исходный код недоступен.
- Варианты: пересборка из исходников (если будут получены), замена на чистую реализацию палитры на iOS, или отказ от MoonMonet.
- `libmoonmonet.so` и `libmoonmonet_rs_arm64.so` байт-идентичны по размеру (505 624) и экспорту; `libmoonmonet_rs_arm.so` — 32-bit вариант.

## 3. Нативные функции NeoLoader (по FFI-объявлениям в Lua)

Подтверждены поиском по Lua-исходникам (без бинарного подтверждения, т.к. `libneomloader.so` отсутствует):

| Функция | Использование | Назначение |
|---|---|---|
| `NeoMLoaderGetJNIEnv()` | `android/jni-raw.lua` | Получение JNIEnv* |
| `NeoMLoaderGetActivity()` | `android/jni-raw.lua` | Получение jobject Activity |
| `NeoMLoader_GetBassPath()` | `bass.lua` | Путь к BASS |
| `NeoMLoader_GetBassSymbol(const char*)` | `bass.lua` | Получение символа BASS |
| `neom_mimgui_create_context(script_name)` | `mimgui/init.lua` | Создание ImGui-контекста |
| `neom_mimgui_create_texture_from_file` | `mimgui/cimgui.lua` | Текстура из файла |
| `neom_mimgui_create_texture_from_memory` | `mimgui/cimgui.lua` | Текстура из памяти |
| `neom_mimgui_get_dpi_scale` | `mimgui/cimgui.lua` | DPI scale |
| `neom_mimgui_invalidate_fonts_texture` | `mimgui/cimgui.lua` | Инвалидация шрифтов |
| `neom_mimgui_push_button_repeat` / `pop_button_repeat` | `mimgui/cimgui.lua` | Button repeat |
| `neom_mimgui_push_font` / `pop_font` | `mimgui/cimgui.lua` | Шрифты |
| `neom_mimgui_pop_style_color` / `pop_style_var` | `mimgui/cimgui.lua` | Стили |
| `neom_mimgui_release_texture` | `mimgui/cimgui.lua` | Освобождение текстуры |
| `neom_mimgui_switch_context` | `mimgui/cimgui.lua` | Переключение контекста |

Все эти функции объявлены через `ffi.cdef` и вызываются через `ffi.C.*`. Их существование в `libneomloader.so` **не подтверждено** (бинарник недоступен). Строки в Lua — не доказательство экспорта.

## 4. JAR-артефакты

### 4.1. `arzapi.jar`
- Размер 1 762 байт, содержит `classes.dex` (2 828 байт).
- Используется в `android/arizona.lua`: `InjectJar(file, "monetloader/ArizonaAPI", "init", "(Lcom/arizona/game/GTASA;)V", activity)`.
- Экспортируемые Java-методы (по Lua-вызовам): `closeCurrentDialog`, `createNotification`, `sampCloseCurrentDialogWith` (через Lua-обёртку), `closeCurrentDialog`.
- Android-only; на iOS не применимо.

### 4.2. `WebViews.jar`
- Размер 18 778 байт, содержит `classes.dex` (51 488 байт) + MANIFEST.MF.
- Используется в `webviews/init.lua`: `InjectJar(..., "WebViews/Main", "init", "(Landroid/app/Activity;)V", activity)`.
- Java-методы (по Lua-вызовам): `createBrowser`, `createBrowserFromFile`, `executeJS`, `changeUrl`, `setClickable`, `setVisible`, `resizeBrowser` (setPos/setSize), `deleteBrowser`, `showCloseButton`, `setSetting`, `addLog`, `sendClick`, `resetUDPPort`, `getUDPPort`, `goPage`, `getStatus`, `getVersion`, `getBrowsers`, `getBrowser`, `getScreen`.
- Android-only; на iOS заменяется WKWebView bridge.

## 5. Android ELF: общие свойства

- Все ELF собраны под Android (Bionic), API 26, ARM/ARM64.
- Зависимости только от стандартных Android-библиотек (`libc`, `libm`, `libdl`).
- Прямое использование на iOS невозможно: формат Mach-O, dyld, code signing, sandbox.
- Требуется пересборка из исходников (если будут получены) или замена.

## 6. Нативные модули NeoLoader (не найдены как файлы)

| Модуль | Упоминание | Назначение | Статус |
|---|---|---|---|
| `NeoSAMemory` | `SAMemory/init.lua` | Игровая память/структуры | Не найден; реализация неизвестна |
| `RakLuaSO` | `RakLua.lua` | RakNet handlers | Не найден |
| `effil` | `scriptmgr.lua`, `copas` | Потоки | Не найден |
| `mime.core` | `mime.lua` | MIME-кодирование | Не найден |
| `cjson` | `require("cjson")` | JSON (C) | Не найден (только `cjson/util.lua`) |
| `bass` | `bass.lua` | Аудио | Загружается через `NeoMLoader_GetBassSymbol` |
| `GTASA` | `ffi.load("GTASA")` | Игровая библиотека | Не найдена как файл (вероятно, `libGTASA.so` игры) |
| `monetloader` | `ffi.load("monetloader")` | ImGui native (по имени) | Не найден |

## 7. Выводы

- Доступный нативный код (MoonMonet) — узкоспециализированная Android ELF с одним экспортом; на iOS не используется напрямую.
- Основные нативные функции NeoLoader (`neom_mimgui_*`, `NeoMLoaderGetJNIEnv/Activity`, `NeoMLoader_GetBass*`) не могут быть проверены бинарно — `libneomloader.so` недоступен.
- JNI-слой и JAR-файлы полностью Android-специфичны; на iOS заменяются нативными bridge-ами.
- Нативные Lua-модули (cjson, mime.core, effil, RakLuaSO, NeoSAMemory) требуют замены или пересборки под iOS.
- Прямое копирование Android ELF в iOS-проект недопустимо и не выполнялось.

## 8. Источники

- `docs/NEOMLOADER_API_INVENTORY.md`
- `docs/ABI_RESEARCH.md`
- `file`, `readelf -d`, `nm -D` для трёх MoonMonet ELF.
- `unzip -l` для JAR.
- `grep` по Lua-исходникам для `ffi.load`, `ffi.C.`, `NeoMLoader_`.
