import 'dart:async';
import 'dart:convert';

import 'package:modern_gauge_flutter/models/alert_info.dart';
import 'package:modern_gauge_flutter/services/log_service.dart';
import 'package:modern_gauge_flutter/utils/app_constants.dart';
import 'package:web_socket_channel/web_socket_channel.dart';

/// Listens to the Go server's /ws/notifications websocket and emits forwarded
/// phone [AlertInfo]s as one-shot events on [alerts].
///
/// Alerts are fire-once: the server sends no initial snapshot on connect and
/// never sends updates or dismissals. Nothing is retained — consumers react to
/// each event (e.g. show a transient banner) as it arrives. Each message is one
/// alert:
/// ```json
/// {"app": "Signal", "title": "Alice", "text": "Hi", "posted_at": 1700000000000}
/// ```
class NotificationServerListener {
  final String wsUrl;

  WebSocketChannel? _channel;
  StreamSubscription? _subscription;
  Timer? _reconnectTimer;
  bool _autoReconnect = false;
  int _reconnectAttempts = 0;

  final StreamController<AlertInfo> _alerts =
      StreamController<AlertInfo>.broadcast();

  static const _reconnectDelays = [1, 2, 4, 8, 16, 30];

  NotificationServerListener({this.wsUrl = AppConstants.notificationsWsUrl});

  /// Broadcast stream of alerts as they arrive. Fire-once events; not replayed.
  Stream<AlertInfo> get alerts => _alerts.stream;

  void start() {
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
      _alerts.add(AlertInfo.fromJson(map));
    } catch (e) {
      LogService.error('NotificationServerListener: parse error: $e');
    }
  }

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

  void dispose() {
    _autoReconnect = false;
    _reconnectTimer?.cancel();
    _reconnectTimer = null;
    _closeCurrentConnection();
    _alerts.close();
  }
}
