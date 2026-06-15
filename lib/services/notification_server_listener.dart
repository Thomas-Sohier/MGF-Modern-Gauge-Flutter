import 'dart:async';
import 'dart:convert';

import 'package:flutter/foundation.dart';
import 'package:modern_gauge_flutter/models/alert_info.dart';
import 'package:modern_gauge_flutter/services/log_service.dart';
import 'package:web_socket_channel/web_socket_channel.dart';

/// Listens to the Go server's /ws/notifications websocket and exposes forwarded
/// phone [AlertInfo]s.
///
/// Alerts are fire-once events: the server sends no initial snapshot on connect
/// and never sends updates or dismissals. Each message is one alert:
/// ```json
/// {"app": "Signal", "title": "Alice", "text": "Hi", "posted_at": 1700000000000}
/// ```
///
/// The listener keeps the most recent alert plus a capped, newest-first history
/// so a future UI can show either the latest alert or a short list.
class NotificationServerListener with ChangeNotifier {
  final String wsUrl;

  /// Maximum number of alerts retained in [recent].
  final int maxHistory;

  WebSocketChannel? _channel;
  StreamSubscription? _subscription;
  Timer? _reconnectTimer;
  bool _autoReconnect = false;
  int _reconnectAttempts = 0;

  final List<AlertInfo> _recent = [];

  static const _reconnectDelays = [1, 2, 4, 8, 16, 30];

  NotificationServerListener({
    this.wsUrl = 'ws://localhost:8080/ws/notifications',
    this.maxHistory = 20,
  });

  /// Most recent alert, or null if none received this session.
  AlertInfo? get latest => _recent.isEmpty ? null : _recent.first;

  /// Recent alerts, newest first, capped at [maxHistory]. Unmodifiable.
  List<AlertInfo> get recent => List.unmodifiable(_recent);

  Future<void> start() async {
    _autoReconnect = true;
    _reconnectAttempts = 0;
    _connect();
  }

  void _connect() {
    _closeCurrentConnection();

    LogService.info('NotificationServerListener: connecting to $wsUrl');
    try {
      _channel = WebSocketChannel.connect(Uri.parse(wsUrl));
    } catch (e) {
      LogService.error('NotificationServerListener: connect failed: $e');
      _scheduleReconnect();
      return;
    }

    _subscription = _channel!.stream.listen(
      (message) => _handleMessage(message as String),
      onError: (e) {
        LogService.error('NotificationServerListener: ws error: $e');
        _handleDisconnect();
      },
      onDone: () {
        LogService.info('NotificationServerListener: ws closed');
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
      _recent.insert(0, AlertInfo.fromJson(map));
      if (_recent.length > maxHistory) {
        _recent.removeRange(maxHistory, _recent.length);
      }
      notifyListeners();
    } catch (e) {
      LogService.error('NotificationServerListener: parse error: $e');
    }
  }

  void _handleDisconnect() {
    if (_channel == null) return;
    _closeCurrentConnection();
    // History is intentionally kept across reconnects: alerts are events, not
    // a live state to clear.
    _scheduleReconnect();
  }

  void _scheduleReconnect() {
    if (!_autoReconnect) return;
    final idx = _reconnectAttempts.clamp(0, _reconnectDelays.length - 1);
    final delay = _reconnectDelays[idx];
    _reconnectAttempts++;
    LogService.info('NotificationServerListener: reconnecting in ${delay}s');
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
