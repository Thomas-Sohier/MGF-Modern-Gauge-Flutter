import 'dart:async';
import 'dart:convert';
import 'dart:ui' as ui;

import 'package:flutter/services.dart';
import 'package:go_router/go_router.dart';
import 'package:modern_gauge_flutter/providers/ecu_provider.dart';
import 'package:modern_gauge_flutter/providers/settings_provider.dart';
import 'package:modern_gauge_flutter/routes/navigation_logic.dart';
import 'package:modern_gauge_flutter/routes/route_names.dart';
import 'package:modern_gauge_flutter/services/head_unit_catalog.dart';
import 'package:modern_gauge_flutter/services/log_service.dart';
import 'package:modern_gauge_flutter/utils/app_constants.dart';
import 'package:web_socket_channel/web_socket_channel.dart';

/// Bridges the head unit's view/settings state to the companion phone via the
/// Go agent's bidirectional `/ws/headunit` socket.
///
/// This app is the single source of truth: it publishes a self-describing
/// catalog (views + settings) whenever its state changes, and applies the
/// phone's commands (switch view, toggle visibility, set a setting), enforcing
/// the invariant that the current view is always visible. The Go agent is a
/// transparent proxy — it forwards the catalog to the phone over BLE and the
/// phone's commands back here.
class HeadUnitControlService {
  final SettingsProvider _settings;
  final EcuProvider _ecu;
  final GoRouter _router;
  final String wsUrl;

  WebSocketChannel? _channel;
  StreamSubscription? _subscription;
  Timer? _reconnectTimer;
  bool _autoReconnect = false;
  int _reconnectAttempts = 0;
  bool _publishScheduled = false;

  static const _reconnectDelays = [1, 2, 4, 8, 16, 30];

  HeadUnitControlService(
    this._settings,
    this._ecu,
    this._router, {
    this.wsUrl = AppConstants.headUnitWsUrl,
  });

  /// Starts the bridge: connects and republishes the catalog whenever the
  /// settings or the active route change.
  void start() {
    _autoReconnect = true;
    _reconnectAttempts = 0;
    _settings.addListener(_schedulePublish);
    _router.routerDelegate.addListener(_schedulePublish);
    _connect();
  }

  void _connect() {
    _closeCurrentConnection();
    LogService.info('HeadUnitControlService: connecting to $wsUrl');
    try {
      _channel = WebSocketChannel.connect(Uri.parse(wsUrl));
    } catch (e) {
      LogService.error('HeadUnitControlService: connect failed: $e');
      _scheduleReconnect();
      return;
    }

    _subscription = _channel!.stream.listen(
      (message) => _handleCommand(message as String),
      onError: (e) {
        LogService.error('HeadUnitControlService: ws error: $e');
        _handleDisconnect();
      },
      onDone: () {
        LogService.info('HeadUnitControlService: ws closed');
        _handleDisconnect();
      },
      cancelOnError: true,
    );

    // Publish the current catalog so a freshly (re)connected agent has state.
    _publishCatalog();
  }

  // ── Outgoing: catalog ──────────────────────────────────────────────────────

  /// Coalesces bursts of changes (a command often mutates settings *and*
  /// navigates) into a single publish on the next microtask.
  void _schedulePublish() {
    if (_publishScheduled) return;
    _publishScheduled = true;
    scheduleMicrotask(() {
      _publishScheduled = false;
      _publishCatalog();
    });
  }

  void _publishCatalog() {
    final channel = _channel;
    if (channel == null) return;
    final s = _settings.settings;
    final catalog = buildHeadUnitCatalog(
      currentLocation: _currentLocation(),
      enabledScreens: s.enabledScreens,
      themeMode: s.themeMode,
      wifiEnabled: s.wifiEnabled,
      notificationDurationSeconds: s.notificationDurationSeconds,
      minNotificationDurationSeconds:
          AppConstants.minNotificationDurationSeconds,
      maxNotificationDurationSeconds:
          AppConstants.maxNotificationDurationSeconds,
    );
    try {
      channel.sink.add(json.encode(catalog));
    } catch (e) {
      LogService.error('HeadUnitControlService: publish failed: $e');
    }
  }

  String _currentLocation() =>
      _router.routerDelegate.currentConfiguration.uri.path;

  // ── Incoming: commands ─────────────────────────────────────────────────────

  void _handleCommand(String raw) {
    _reconnectAttempts = 0;
    try {
      final cmd = json.decode(raw) as Map<String, dynamic>;
      switch (cmd['type']) {
        case 'set_current_view':
          _setCurrentView(cmd['view_id'] as String?);
        case 'set_view_visibility':
          _setViewVisibility(
            cmd['view_id'] as String?,
            cmd['visible'] as bool?,
          );
        case 'set_setting_value':
          _setSettingValue(cmd['setting_id'] as String?, cmd['value']);
        case 'request_catalog':
          _publishCatalog();
        case 'nav_key':
          _handleNavKey(cmd['key'] as String?);
        default:
          LogService.error('HeadUnitControlService: unknown command: $raw');
      }
    } catch (e) {
      LogService.error('HeadUnitControlService: command parse error: $e');
    }
  }

  /// Applies a remote-pad key press from the phone. Screen cycling and leaving
  /// the settings page are handled via the router directly; arrows/OK are
  /// injected as synthesized hardware key events so they behave exactly like a
  /// plugged-in keyboard (the dashboard shell and any focused widget react).
  void _handleNavKey(String? key) {
    final enabledScreens = _settings.settings.enabledScreens;
    final location = _currentLocation();
    switch (key) {
      case 'next':
        if (location.startsWith(RouteNames.dashboardRoute)) {
          _router.go(getNextRoute(location, enabledScreens));
        }
      case 'previous':
        if (location.startsWith(RouteNames.dashboardRoute)) {
          _router.go(getPreviousRoute(location, enabledScreens));
        }
      case 'back':
        if (location == RouteNames.settingsRoute) {
          _router.go(buildDashboardRoutes(enabledScreens).first);
        } else {
          _pressKey(LogicalKeyboardKey.escape, PhysicalKeyboardKey.escape);
        }
      case 'ok':
        _pressKey(LogicalKeyboardKey.enter, PhysicalKeyboardKey.enter);
      case 'up':
        _pressKey(LogicalKeyboardKey.arrowUp, PhysicalKeyboardKey.arrowUp);
      case 'down':
        _pressKey(LogicalKeyboardKey.arrowDown, PhysicalKeyboardKey.arrowDown);
      case 'left':
        _pressKey(LogicalKeyboardKey.arrowLeft, PhysicalKeyboardKey.arrowLeft);
      case 'right':
        _pressKey(
          LogicalKeyboardKey.arrowRight,
          PhysicalKeyboardKey.arrowRight,
        );
      default:
        LogService.error('HeadUnitControlService: unknown nav key: $key');
    }
  }

  void _pressKey(LogicalKeyboardKey logical, PhysicalKeyboardKey physical) {
    // On récupère le dispatcher qui fait le pont entre le moteur C++ et Flutter
    final dispatcher = ui.PlatformDispatcher.instance;
    final onKeyData = dispatcher.onKeyData;

    if (onKeyData == null) {
      LogService.error('HeadUnitControlService: onKeyData est null');
      return;
    }
    LogService.info('Received input. Logical : $logical, physical : $physical');

    final ts = Duration(milliseconds: DateTime.now().millisecondsSinceEpoch);

    // 1. On simule l'appui de la touche (Key Down)
    onKeyData(
      ui.KeyData(
        type: ui.KeyEventType.down,
        physical: physical.usbHidUsage,
        logical: logical.keyId,
        timeStamp: ts,
        character: null,
        synthesized: false,
      ),
    );

    // 2. On simule le relâchement de la touche (Key Up)
    onKeyData(
      ui.KeyData(
        type: ui.KeyEventType.up,
        physical: physical.usbHidUsage,
        logical: logical.keyId,
        timeStamp: ts,
        character: null,
        synthesized: false,
      ),
    );
  }

  void _setCurrentView(String? viewId) {
    if (viewId == null) return;
    final view = _viewBySegment(viewId);
    if (view == null) return;
    _router.go(view.fullPath);
  }

  void _setViewVisibility(String? viewId, bool? visible) {
    if (viewId == null || visible == null) return;
    if (_viewBySegment(viewId) == null) return;
    _settings.setScreenEnabled(viewId, visible);

    // Invariant: the current view is never hidden. If we just hid the visible
    // view that is on screen, switch to the first remaining visible one.
    if (!visible &&
        _currentLocation() == '${RouteNames.dashboardRoute}$viewId') {
      final routes = buildDashboardRoutes(_settings.settings.enabledScreens);
      _router.go(routes.first);
    }
  }

  void _setSettingValue(String? settingId, Object? value) {
    if (settingId == null || value == null) return;
    switch (settingId) {
      case HeadUnitSettingIds.theme:
        final mode = themeModeFromWire(value.toString());
        if (mode != null) _settings.setThemeMode(mode);
      case HeadUnitSettingIds.wifi:
        if (value is bool) {
          _settings.setWifiEnabled(value);
          _ecu.setWifiEnabled(value);
        }
      case HeadUnitSettingIds.notificationDuration:
        if (value is num) {
          _settings.setNotificationDurationSeconds(value.toInt());
        }
      default:
        LogService.error('HeadUnitControlService: unknown setting: $settingId');
    }
  }

  HeadUnitView? _viewBySegment(String segment) {
    for (final v in headUnitViews) {
      if (v.segment == segment) return v;
    }
    return null;
  }

  // ── Connection lifecycle ───────────────────────────────────────────────────

  void _handleDisconnect() {
    if (_channel == null) return;
    _closeCurrentConnection();
    _scheduleReconnect();
  }

  void _scheduleReconnect() {
    if (!_autoReconnect) return;
    final idx = _reconnectAttempts.clamp(0, _reconnectDelays.length - 1);
    final delay = _reconnectDelays[idx];
    _reconnectAttempts++;
    LogService.info('HeadUnitControlService: reconnecting in ${delay}s');
    _reconnectTimer?.cancel();
    _reconnectTimer = Timer(Duration(seconds: delay), _connect);
  }

  void _closeCurrentConnection() {
    _subscription?.cancel();
    _subscription = null;
    final ch = _channel;
    _channel = null;
    try {
      ch?.sink.close();
    } catch (_) {}
  }

  void dispose() {
    _autoReconnect = false;
    _settings.removeListener(_schedulePublish);
    _router.routerDelegate.removeListener(_schedulePublish);
    _reconnectTimer?.cancel();
    _reconnectTimer = null;
    _closeCurrentConnection();
  }
}
