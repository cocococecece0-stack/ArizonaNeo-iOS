# Stage 13.3 — Production UIKit Entry Point Discovery and Documents Picker Integration

## Preflight
- Branch `stage-7.1.7-device-validation`; HEAD `eb67b99b2511bd9c3c4689262180b3331dd689a3`; `main` `889f3b2…`.
- Dirty worktree: только локальные документы пользователя (сохранены).

## Исследование реального Arizona executable
- Mach-O arm64, `UIApplication`/UIKit/SwiftUI/WebKit присутствуют как framework dependencies.
- ObjC-классы собственного приложения: только `BContext`, `BSurface` (EAGL-обвязка). Публичных `UIViewController`/`UIApplication` subclass для Arizona не обнаружено.
- Селекторы UIKit/SwiftUI потребительские; нет подтверждённого hook для добавления picker.
- `__mod_init_func`/`+load` отсутствуют; entrypoint — standard `UIApplicationMain` + SwiftUI scene lifecycle.
- `_ui_set_handler` (0x100925bf8) существует, но его вызов из `_game_boot` не локализован; ABI callback не доказан.
- `_game_boot` вызывает только internal loader 5 hardcoded библиотек; script registration interface отсутствует.

Вывод: **безопасный production entrypoint для `UIDocumentPickerViewController` в оригинальном Arizona не подтверждён**.

## Реализовано (без фиктивного UIKit host)
- `ios/host/ios_documents_host.hpp/.mm` — реальная реализация `DocumentsAccess`/`BookmarkStore` на Foundation/UIKit:
  - `create_bookmark`: `NSURLBookmarkCreationWithSecurityScope`
  - `resolve_bookmark`: `NSURLBookmarkResolutionWithSecurityScope`, `startAccessingSecurityScopedResource`
  - `save/load`: `NSUserDefaults`
  - `present_neoloader_folder_picker`: `UIDocumentPickerViewController` с `UTTypeFolder`, `asCopy:NO`, delegate completion
- `neo_ios_documents_host` CMake target (Apple-only) с frameworks Foundation/UIKit/UniformTypeIdentifiers.
- Core остаётся UIKit-free; adapter только bridge.

## Интеграция в Arizona
Не выполнена: нет подтверждённого production entrypoint, кнопки/экрана или callback, через который можно вызвать picker. Создавать фиктивный UIKit host запрещено.

## Конкретный недостающий компонент
1. Исходный код Arizona RP iOS или подтверждённый ObjC/Swift entrypoint (например, `UIApplicationDelegate.application(_:didFinishLaunchingWithOptions:)`), в котором можно вызвать `present_neoloader_folder_picker` и затем `UserDocumentsRoot::select()`.
2. Документированный способ регистрации собственного UI-компонента в SwiftUI/UIKit scene Arizona.
3. Без этого безопасный picker недостижим, а любая попытка использовать `_ui_set_handler` — неподтверждённый ABI.

## Проверки
- Linux Debug/Release + LuaJIT ON: 14/14 PASS.
- Linux LuaJIT OFF: 11/11 PASS.
- iOS Core build (run `45685236878`): success.
- iOS Core + host adapter build (run `45687742319`): success (Foundation/UIKit/UniformTypeIdentifiers link OK).
- Пользовательские документы и локальные изменения сохранены.

## Git
Branch `stage-7.1.7-device-validation`; `main` не изменён. Изменения будут закоммичены после финальной проверки.

## Вердикт
**BLOCKED** для реального picker в Arizona. Реализован production-ready adapter, но его активация требует подтверждённого entrypoint в original Arizona executable/source. Не заявлять DEVICE_LOAD_PASS или RUNTIME_READY.
