# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What This App Does

Modern Gauge Flutter is a vehicle diagnostic dashboard app that displays real-time engine telemetry through gauge visualizations. It connects to a Go backend agent via WebSocket on `localhost:8080` that reads from an ECU (Engine Control Unit). Designed as an in-car display with music player integration (Linux MPRIS/D-Bus).

## Commands

```bash
# Install dependencies
flutter pub get

# Run the app
flutter run

# Analyze code
flutter analyze

# Run tests
flutter test
flutter test test/widget_test.dart

# Regenerate Mockito mocks after changing @GenerateMocks annotations
flutter pub run build_runner build --delete-conflicting-outputs

# Build
flutter build linux
flutter build apk
```

## Architecture

**Data flow:**
1. `EcuService` — WebSocket client to Go backend (`ws://localhost:8080/ws`); emits `Stream<EcuInfos>` of raw 40+ field sensor data
2. `OdbService` — Wraps EcuService; parses `EcuInfos` into `DialData` (rpm, speed, coolantTemp, fuelLevel, oilPressure, batteryVoltage, odometer); emits `Stream<DialData>` and `Stream<OdbConnectionStatus>`
3. **Providers** — Listen to service streams and call `notifyListeners()` for UI rebuilds:
   - `DialProvider` — Exposes current `DialData`
   - `AppStateProvider` — Sleep mode, ODB connection status, global alerts
   - `EcuProvider` — WebSocket connection control and retry logic
   - `SettingsProvider` — User preferences synced with `SettingsService` (SharedPreferences)
   - `MprisProvider` — Linux D-Bus media player state
4. **UI** — Screens use `Consumer<>` / `Selector<>` to consume providers

**Routing:** `go_router` with a `ShellRoute` wrapping the dashboard screens (rpm, clock, music, settings). Navigation between screens uses left/right arrow keys handled in `DashboardShellScreen`.

**Screen order for keyboard navigation:** splash → rpm → clock → music → settings (defined in `navigation_logic.dart`)

**Services are singletons** (`LogService`, `SettingsService`). Initialization order in `main.dart` matters: LogService → SettingsService → EcuService → OdbService → providers → router → MprisListener.

**Platform notes:** MPRIS/D-Bus integration (music player) only works on Linux. The `MprisListener` is skipped silently on other platforms. Logs are stored in the OS app support directory (e.g., `~/.local/share/modern_gauge_flutter/logs/` on Linux).

## Key Conventions

- Models use `copyWith()` pattern (`DialData`, `EcuData`, `SettingsData`)
- Widgets prefer `Selector<>` over `Consumer<>` for fine-grained rebuilds
- `LogService` is used throughout for logging (not `print`)
- French comments appear in some files — this is intentional

## Known Gaps

- Tests in `widget_test.dart` are currently skipped (`skip: true`) — mock infrastructure exists but tests need work
- `fuelLevel`, `oilPressure`, and `odometer` fields in `DialData` are hardcoded to `0.0` — not available in the current ECU JSON payload
