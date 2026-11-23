# Core Infrastructure (Stage 5)

Документация по переносимой C++ Core инфраструктуре, добавленной в Stage 5.

## 1. Компоненты

### 1.1. Logger (`include/arizona_neo/core/logger.hpp`)
- `enum class LogLevel { Trace, Debug, Info, Warning, Error }`.
- `to_string(LogLevel)` — стабильное строковое представление.
- `Logger` — потокобезопасный, без глобального состояния.
- `set_min_level`, `min_level`, `set_sink`, `log` + удобные методы.
- Пользовательский sink никогда не вызывается под внутренним mutex.
- Исключения sink перехватываются и выводятся в stderr (best effort).
- Фильтрация по минимальному уровню.

### 1.2. SandboxedFilesystem (`include/arizona_neo/core/filesystem.hpp`)
- Работает только внутри заданного root.
- Операции: `exists`, `create_directories`, `read_file`, `write_file`, `list_directory`, `remove_file`.
- Отклоняет абсолютные пути, `..`, symlink escape (best effort через weakly_canonical).
- Не удаляет root, не удаляет директории через `remove_file`.
- Ограничения: не TOCTOU-proof; std::filesystem не предоставляет O_NOFOLLOW. Документировано.

### 1.3. Scheduler (`include/arizona_neo/core/scheduler.hpp`)
- Детерминированный, single-threaded, driven by `tick(delta)`.
- `schedule_after`, `schedule_periodic`, `cancel`, `tick`.
- Идентификаторы задач, отмена, однократные и периодические задачи.
- Предсказуемое выполнение; callback-исключения изолированы.
- Защита от периодических задач с period <= 0.
- Нет скрытых worker threads, нет платформенных таймеров.

### 1.4. EventBus (`include/arizona_neo/core/event_bus.hpp`)
- Подписка на именованные события, токен подписки, отписка по токену.
- Безопасность при отписке/подписке во время dispatch (snapshot semantics).
- Изоляция исключений обработчиков.
- Несколько обработчиков одного события.
- Без глобального singleton.

### 1.5. ScriptManager (`include/arizona_neo/core/script_manager.hpp`)
- Состояния: Discovered, Loaded, Running, Stopped, Failed.
- Регистрация/удаление/список/получение скриптов, метаданные (id, name, version, author, description, path).
- Проверка допустимости переходов.
- `run()` возвращает `CoreError::RuntimeUnavailable` — Lua Runtime не реализован, фиктивного выполнения нет.

## 2. Обработка ошибок
- `Result<T>` / `Result<void>` с `CoreError` и сообщением.
- Явные коды ошибок, без исключений для ожидаемых сбоев.

## 3. Потоковая модель
- Logger: mutex защищает состояние; sink вызывается вне mutex.
- SandboxedFilesystem: stateless относительно потоков, но std::filesystem не потокобезопасен для одной и той же файловой системы; предполагается внешняя синхронизация при параллельных операциях.
- Scheduler: mutex защищает tasks_; callbacks вызываются вне mutex. Не thread-safe для одновременного tick из нескольких потоков.
- EventBus: mutex защищает entries_; handlers вызываются вне mutex.
- ScriptManager: mutex защищает scripts_.

## 4. Ограничения безопасности Filesystem
- Best-effort, не TOCTOU-proof. Документировано в filesystem.hpp.

## 5. Отсутствие Lua Runtime
- `LuaHost::execute` не реализован (lua_host.cpp пуст).
- ScriptManager::run возвращает RuntimeUnavailable.
- Никакого фиктивного выполнения скриптов.

## 6. Дальнейшее подключение платформенных адаптеров
- iOS-адаптеры (platform/ios/) будут реализованы в следующих этапах.
- GameBridge — только после подтверждённых точек входа iOS-клиента.

## 7. Тесты
- 6 тестовых бинарников: version_test, logger_test, filesystem_test, scheduler_test, event_bus_test, script_manager_test.
- Все проходят в Debug и Release.
