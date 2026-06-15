import 'dart:async';
import 'dart:convert';

import 'package:flutter/foundation.dart';
import 'package:modern_gauge_flutter/models/navigation_info.dart';
import 'package:modern_gauge_flutter/services/log_service.dart';
import 'package:modern_gauge_flutter/utils/app_constants.dart';
import 'package:web_socket_channel/web_socket_channel.dart';

/// Listens to the Go server's /ws/navigation websocket and exposes the current
/// turn-by-turn [NavigationInfo].
///
/// The server pushes a JSON snapshot on connect and on every change:
/// ```json
/// {
///   "navigation": {
///     "active": true, "instruction": "Turn right onto ...",
///     "distance": "200 m", "eta": "14:32", "maneuver_icon_id": "abc"
///   },
///   "icon_id": "abc",
///   "has_icon": true
/// }
/// ```
///
/// When `has_icon` is true the maneuver PNG is available at
/// GET /api/navigation/icon; [NavigationInfo.iconUrl] points there with the
/// icon id appended so Flutter's image cache invalidates on each maneuver.
class NavigationServerListener with ChangeNotifier {
  final String wsUrl;
  final String iconUrl;

  WebSocketChannel? _channel;
  StreamSubscription? _subscription;
  Timer? _reconnectTimer;
  bool _autoReconnect = false;
  int _reconnectAttempts = 0;

  NavigationInfo _navigation = const NavigationInfo.inactive();

  static const _reconnectDelays = [1, 2, 4, 8, 16, 30];

  NavigationServerListener({
    this.wsUrl = AppConstants.navigationWsUrl,
    this.iconUrl = AppConstants.navigationIconUrl,
  });

  /// Current navigation state. Never null; defaults to inactive.
  NavigationInfo get navigation => _navigation;

  bool get isNavigating => _navigation.active;

  Future<void> start() async {
    _autoReconnect = true;
    _reconnectAttempts = 0;
    _connect();
  }

  void _connect() {
    _closeCurrentConnection();

    LogService.info('NavigationServerListener: connecting to $wsUrl');
    try {
      _channel = WebSocketChannel.connect(Uri.parse(wsUrl));
    } catch (e) {
      LogService.error('NavigationServerListener: connect failed: $e');
      _scheduleReconnect();
      return;
    }

    _subscription = _channel!.stream.listen(
      (message) => _handleMessage(message as String),
      onError: (e) {
        LogService.error('NavigationServerListener: ws error: $e');
        _handleDisconnect();
      },
      onDone: () {
        LogService.info('NavigationServerListener: ws closed');
        _handleDisconnect();
      },
      cancelOnError: true,
    );
  }

  void _handleMessage(String raw) {
    try {
      // A message means the connection is healthy; reset backoff so a later
      // disconnect reconnects quickly instead of using the max delay.
      _reconnectAttempts = 0;
      final map = json.decode(raw) as Map<String, dynamic>;
      _navigation = NavigationInfo.fromSnapshot(map, iconBaseUrl: iconUrl);
      notifyListeners();
    } catch (e) {
      LogService.error('NavigationServerListener: parse error: $e');
    }
  }

  void _handleDisconnect() {
    if (_channel == null) return;
    _closeCurrentConnection();
    // Clear navigation so the "no navigation" UI shows.
    _navigation = const NavigationInfo.inactive();
    notifyListeners();
    _scheduleReconnect();
  }

  void _scheduleReconnect() {
    if (!_autoReconnect) return;
    final idx = _reconnectAttempts.clamp(0, _reconnectDelays.length - 1);
    final delay = _reconnectDelays[idx];
    _reconnectAttempts++;
    LogService.info('NavigationServerListener: reconnecting in ${delay}s');
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

  @override
  void dispose() {
    _autoReconnect = false;
    _reconnectTimer?.cancel();
    _reconnectTimer = null;
    _closeCurrentConnection();
    super.dispose();
  }
}
