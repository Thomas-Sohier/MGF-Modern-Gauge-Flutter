# MGF — Modern Gauge Flutter

In-car vehicle diagnostic dashboard built with Flutter. Connects to a Go backend via WebSocket to display real-time ECU telemetry through animated gauge visualizations.

## Features

- Animated segmented RPM / temperature / voltage gauges
- Analog clock (minute-aligned, no seconds)
- Music player integration via Linux MPRIS / D-Bus
- Settings screen with per-category pages
- Left/right arrow key navigation between screens

## Architecture

```
ECU WebSocket → EcuService → OdbService → Providers → UI
```

| Layer | Responsibility |
|---|---|
| `EcuService` | WebSocket client (`ws://localhost:8080/ws`), raw `EcuInfos` stream |
| `OdbService` | Parses `EcuInfos` into typed `DialData` |
| Providers | `DialProvider`, `AppStateProvider`, `EcuProvider`, `SettingsProvider`, `MprisProvider` |
| UI | Screens consume providers via `Selector<>` / `Consumer<>` |

Routing: `go_router` with a `ShellRoute`.

## Requirements

- Flutter SDK
- Go backend agent running on `localhost:8080`
- Linux (for MPRIS music integration)

## Commands

```bash
flutter pub get          # install dependencies
flutter run              # run the app
flutter analyze          # static analysis
flutter test             # run tests
flutter build linux      # build for Linux
flutter build apk        # build for Android

# Regenerate mocks after changing @GenerateMocks
flutter pub run build_runner build --delete-conflicting-outputs
```

## Startup options

Compile-time flags (`--dart-define`, work with `flutter run` and `flutter build`):

| Flag | Default | Description |
|------|---------|-------------|
| `SERVER_HOST` | `localhost` | Host of the Go agent (WebSocket + HTTP API) |
| `SERVER_PORT` | `8080` | Port of the Go agent |
| `FAKE_NOW_PLAYING` | `false` | Replace the now-playing WebSocket source with a fake in-app player (debug the music screen without the Go server) |

```bash
flutter run --dart-define=SERVER_HOST=10.0.0.39 --dart-define=SERVER_PORT=8080
flutter run --dart-define=FAKE_NOW_PLAYING=true
```

Runtime environment variables:

| Variable | Default | Description |
|----------|---------|-------------|
| `MGF_LOG_DIR` | `/data/.local/share/flutter-pi/logs` | Log file directory (e.g. set via systemd `Environment=`) |

## Running on Raspberry Pi 3B+

Build in release mode for optimal performance:

```bash
flutter build linux --release --tree-shake-icons --target-platform=linux-arm
```

Debug builds are 3-5× slower on the Pi's limited hardware.

Runtime environment suggestion until Impeller-Linux-embedded is stable for VideoCore IV:

```bash
FLUTTER_ENGINE_SWITCH_IMPELLER=false ./build/linux/arm/release/bundle/modern_gauge_flutter
```

## Project structure

```
lib/
  main.dart
  app.dart
  models/          # EcuData, DialData, SettingsData
  services/        # EcuService, OdbService, LogService, SettingsService, MprisListener
  providers/       # DialProvider, AppStateProvider, EcuProvider, SettingsProvider, MprisProvider
  routes/          # go_router setup, route names, navigation logic
  mixins/          # ScreenNavigationMixin
  ui/
    screens/       # one file per screen + settings sub-pages
    widgets/       # reusable widgets (gauge, clock, music dial, settings/)
    themes/        # ThemeExtensions for gauge, clock, background
```
