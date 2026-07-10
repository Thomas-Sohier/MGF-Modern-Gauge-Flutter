import 'dart:async';
import 'dart:convert';
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

  /// Correspondance touche distante → touche clavier. Le pavé du téléphone est
  /// traduit en événements clavier synthétisés : toute la logique (cycle des
  /// écrans, hiérarchie des settings…) vit dans les handlers clavier des
  /// écrans, identiques pour un clavier branché.
  static const _navKeyMap = {
    'next': (LogicalKeyboardKey.arrowRight, PhysicalKeyboardKey.arrowRight),
    'previous': (LogicalKeyboardKey.arrowLeft, PhysicalKeyboardKey.arrowLeft),
    'settings': (LogicalKeyboardKey.arrowDown, PhysicalKeyboardKey.arrowDown),
    'back': (LogicalKeyboardKey.escape, PhysicalKeyboardKey.escape),
    'ok': (LogicalKeyboardKey.enter, PhysicalKeyboardKey.enter),
    'up': (LogicalKeyboardKey.arrowUp, PhysicalKeyboardKey.arrowUp),
    'down': (LogicalKeyboardKey.arrowDown, PhysicalKeyboardKey.arrowDown),
    'left': (LogicalKeyboardKey.arrowLeft, PhysicalKeyboardKey.arrowLeft),
    'right': (LogicalKeyboardKey.arrowRight, PhysicalKeyboardKey.arrowRight),
  };

  void _handleNavKey(String? key) {
    final mapped = _navKeyMap[key];
    if (mapped == null) {
      LogService.error('HeadUnitControlService: unknown nav key: $key');
      return;
    }
    _pressKey(mapped.$1, mapped.$2);
  }

  /// Injecte la touche au niveau framework (FocusManager) via
  /// [KeyEventManager.keyMessageHandler]. On n'utilise pas
  /// `PlatformDispatcher.onKeyData` : dès qu'un vrai clavier a envoyé un
  /// événement par le canal `flutter/keyevent`, le framework verrouille ce
  /// mode de transit et ignore les KeyData injectés côté moteur.
  // KeyEventManager/KeyMessage sont dépréciés mais restent, dans cette version
  // du SDK, le seul point d'entrée du FocusManager pour injecter des touches.
  // ignore_for_file: deprecated_member_use
  void _pressKey(LogicalKeyboardKey logical, PhysicalKeyboardKey physical) {
    final handler = ServicesBinding.instance.keyEventManager.keyMessageHandler;
    if (handler == null) {
      LogService.error('HeadUnitControlService: keyMessageHandler est null');
      return;
    }
    LogService.info('Received input. Logical : $logical, physical : $physical');

    final ts = Duration(milliseconds: DateTime.now().millisecondsSinceEpoch);

    handler(
      KeyMessage([
        KeyDownEvent(
          physicalKey: physical,
          logicalKey: logical,
          timeStamp: ts,
        ),
      ], null),
    );
    handler(
      KeyMessage([
        KeyUpEvent(physicalKey: physical, logicalKey: logical, timeStamp: ts),
      ], null),
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
