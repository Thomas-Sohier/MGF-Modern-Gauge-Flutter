# CLAUDE.md

Vehicle diagnostic dashboard displaying real-time ECU telemetry via gauges. Connects to Go backend (WebSocket `localhost:8080`). Linux MPRIS/D-Bus music player integration.

## Commands

```bash
flutter pub get                                              # Install deps
flutter run                                                  # Run app
flutter analyze                                              # Lint check
flutter test                                                 # Run tests
flutter pub run build_runner build --delete-conflicting-outputs  # Regen mocks
flutter build linux                                          # Build Linux
```

## Structure

```
lib/
├── main.dart                 # Entry point, DI setup
├── app.dart                  # MaterialApp + Router
├── models/                   # Data classes (EcuData, SettingsData)
├── services/                 # Business logic, external APIs
│   ├── ecu_service.dart      # WebSocket client → Stream<EcuInfos>
│   ├── settings_service.dart # SharedPreferences persistence
│   ├── log_service.dart      # Structured logging
│   └── mpris_listener.dart   # Linux D-Bus media player
├── providers/                # ChangeNotifiers (ViewModels)
│   ├── ecu_provider.dart     # Holds EcuInfos, throttles to ~6Hz
│   ├── settings_provider.dart
│   ├── app_state_provider.dart
│   └── mpris_provider.dart
├── routes/                   # go_router config
├── ui/
│   ├── screens/              # Full-page views
│   ├── widgets/              # Reusable components
│   └── themes/               # ThemeData, text styles
├── mixins/                   # Shared widget behavior
└── utils/                    # Constants, helpers
```

## MVVM Architecture

```
┌─────────────┐    Stream     ┌─────────────┐  notifyListeners  ┌─────────────┐
│   Service   │ ───────────► │  Provider   │ ─────────────────► │    View     │
│  (Model)    │              │ (ViewModel) │                    │  (Screen)   │
└─────────────┘              └─────────────┘                    └─────────────┘
```

- **Model**: Services + data classes. `EcuService` emits `Stream<EcuInfos>`, `SettingsService` persists to SharedPreferences
- **ViewModel**: Providers extend `ChangeNotifier`. Subscribe to service streams, expose state, call `notifyListeners()`
- **View**: Screens use `Selector<Provider, T>` for fine-grained rebuilds

**Init order** (main.dart): LogService → SettingsService → EcuService → Providers → Router → MprisListener

## Data Flow

1. `EcuService` receives JSON via WebSocket → parses to `EcuData` (40+ sensor fields)
2. `EcuProvider` subscribes, holds current state, throttles UI updates to ~6Hz
3. Screens use `Selector<EcuProvider, double>` to extract single fields (rpm, temp, etc.)
4. Only affected widgets rebuild

## Companion remote control

This app is the **source of truth** for the head unit's views and settings, and
the companion phone controls them via the Go agent's bidirectional
`/ws/headunit` socket. `HeadUnitControlService` (started in `app.dart`'s provider
tree) publishes a self-describing catalog — the dashboard views (id = route
segment, visibility from `enabledScreens`, `current` from the active route) plus
three settings (theme/enum, wifi/bool, notification duration/number) — whenever
settings or the active route change, and applies the phone's commands
(`set_current_view` → `go_router`, `set_view_visibility` → `SettingsProvider`,
`set_setting_value`, `request_catalog`). It enforces the invariant that the
current view is always visible (hiding the on-screen view switches away first).
Pure catalog/enum logic lives in `head_unit_catalog.dart` (unit-tested); the
service handles WebSocket IO and state mutation.

## Key Conventions

### Dart/Flutter (Official Rules)

- **Naming**: `PascalCase` classes, `camelCase` members, `snake_case` files
- **Line length**: 80 chars max
- **Functions**: <20 lines, single purpose, arrow syntax for one-liners
- **Null safety**: Avoid `!` unless guaranteed non-null
- **Async**: `Future` + `async/await` for single ops, `Stream` for sequences
- **Widgets**: Immutable, use `const` constructors, prefer composition over inheritance
- **State**: `Selector<>` over `Consumer<>` for targeted rebuilds
- **Lists**: Use `ListView.builder` for long lists
- **Docs**: `///` dartdoc, first sentence is summary, document public APIs only

### Project-Specific

- Models use `copyWith()` pattern
- `LogService` for logging (never `print`)
- French comments are intentional
- Singletons: `LogService`, `SettingsService`
- Platform: MPRIS works only on Linux, skipped elsewhere

## Routing

`go_router` with `ShellRoute` wrapping dashboard screens.

**Screen order** (keyboard nav): splash → rpm → clock → music → settings

Arrow keys handled in `DashboardShellScreen`, logic in `navigation_logic.dart`.

## Testing

```bash
flutter test                           # Unit + widget tests
flutter test test/widget_test.dart     # Specific file
```

- Use `package:flutter_test` for widgets
- Prefer fakes/stubs over mocks
- Arrange-Act-Assert pattern
- Regenerate mocks after `@GenerateMocks` changes

## Theming

- Centralized in `ui/themes/`
- `AppTheme` defines `ThemeData`
- Component themes: `GaugeTheme`, `ClockTheme`
- Access via `Theme.of(context)`

## Error Handling

- Services handle WebSocket reconnection
- Providers catch stream errors
- `LogService` logs with severity levels
- Never let errors fail silently

## Performance

- `EcuProvider` throttles to ~6Hz via `SchedulerBinding`
- Widgets extract single values via `Selector<>`
- `const` widgets where possible
- Avoid expensive ops in `build()`
