import 'dart:async';
import 'dart:convert';

import 'package:flutter/foundation.dart';
import 'package:modern_gauge_flutter/providers/mpris_provider.dart';
import 'package:modern_gauge_flutter/services/log_service.dart';
import 'package:modern_gauge_flutter/models/media_info.dart';
import 'package:modern_gauge_flutter/utils/app_constants.dart';
import 'package:web_socket_channel/web_socket_channel.dart';

/// Listens to the Go server's /ws/nowplaying websocket and exposes
/// the now-playing state via [MprisListenerBase].
///
/// The server pushes a JSON snapshot on connect and on every change:
/// ```json
/// {
///   "metadata": {
///     "title": "...", "artist": "...", "album": "...",
///     "state": "playing|paused|stopped",
///     "position_ms": 12345, "duration_ms": 240000,
///     "art_id": "abc"
///   },
///   "art_id": "abc",
///   "has_art": true
/// }
/// ```
///
/// When [hasArt] is true, album art is available at
/// GET /api/nowplaying/art. The [MediaInfo.artUrl] is set to that URL
/// with the art_id appended as a query parameter so Flutter's image
/// cache is invalidated whenever the art changes.
class NowPlayingServerListener
    with ChangeNotifier
    implements MprisListenerBase {
  final String wsUrl;
  final String artUrl;

  WebSocketChannel? _channel;
  StreamSubscription? _subscription;
  Timer? _reconnectTimer;
  bool _autoReconnect = false;
  int _reconnectAttempts = 0;

  // Position tracking timer (mirrors MprisListener behaviour)
  Timer? _positionTimer;
  static const _positionUpdateInterval = Duration(milliseconds: 500);

  MediaInfo? _mediaInfo;
  Duration _position = Duration.zero;
  PlaybackStatus _playbackStatus = PlaybackStatus.stopped;

  static const _reconnectDelays = [1, 2, 4, 8, 16, 30];

  NowPlayingServerListener({
    this.wsUrl = AppConstants.nowPlayingWsUrl,
    this.artUrl = AppConstants.nowPlayingArtUrl,
  });

  @override
  MediaInfo? get mediaInfo => _mediaInfo;

  @override
  Duration get position => _position;

  @override
  PlaybackStatus get playbackStatus => _playbackStatus;

  @override
  bool get isPlaying => _playbackStatus == PlaybackStatus.playing;

  @override
  Future<void> start() async {
    _autoReconnect = true;
    _reconnectAttempts = 0;
    _connect();
  }

  void _connect() {
    _closeCurrentConnection();

    LogService.info('NowPlayingServerListener: connecting to $wsUrl');
    try {
      _channel = WebSocketChannel.connect(Uri.parse(wsUrl));
    } catch (e) {
      LogService.error('NowPlayingServerListener: connect failed: $e');
      _scheduleReconnect();
      return;
    }

    _subscription = _channel!.stream.listen(
      (message) {
        _handleMessage(message as String);
      },
      onError: (e) {
        LogService.error('NowPlayingServerListener: ws error: $e');
        _handleDisconnect();
      },
      onDone: () {
        LogService.info('NowPlayingServerListener: ws closed');
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
      final meta = map['metadata'] as Map<String, dynamic>? ?? {};
      final hasArt = map['has_art'] as bool? ?? false;
      final artId = map['art_id'] as String? ?? '';

      final title = meta['title'] as String?;
      final artist = meta['artist'] as String?;
      final album = meta['album'] as String?;
      final stateStr = meta['state'] as String? ?? 'stopped';
      final positionMs = meta['position_ms'] as int? ?? 0;
      final durationMs = meta['duration_ms'] as int? ?? 0;

      final newStatus = _parseState(stateStr);
      final newPosition = Duration(milliseconds: positionMs);
      final artImageUrl = (hasArt && artId.isNotEmpty)
          ? '$artUrl?artId=${Uri.encodeComponent(artId)}'
          : null;

      _mediaInfo = MediaInfo(
        title: (title?.isNotEmpty ?? false) ? title : null,
        artist: (artist?.isNotEmpty ?? false) ? artist : null,
        album: (album?.isNotEmpty ?? false) ? album : null,
        artUrl: artImageUrl,
        duration: Duration(milliseconds: durationMs),
      );
      _position = newPosition;

      final wasPlaying = isPlaying;
      _playbackStatus = newStatus;

      if (isPlaying && !wasPlaying) {
        _startPositionTimer();
      } else if (!isPlaying && wasPlaying) {
        _stopPositionTimer();
      }

      notifyListeners();
    } catch (e) {
      LogService.error('NowPlayingServerListener: parse error: $e');
    }
  }

  void _startPositionTimer() {
    _stopPositionTimer();
    _positionTimer = Timer.periodic(_positionUpdateInterval, (_) {
      _position += _positionUpdateInterval;
      final duration = _mediaInfo?.duration ?? Duration.zero;
      if (duration > Duration.zero && _position > duration) {
        _position = duration;
        _stopPositionTimer();
      }
      notifyListeners();
    });
  }

  void _stopPositionTimer() {
    _positionTimer?.cancel();
    _positionTimer = null;
  }

  void _handleDisconnect() {
    if (_channel == null) return;
    _closeCurrentConnection();
    // Reset state to stopped so the "no player" UI shows
    _mediaInfo = null;
    _position = Duration.zero;
    _playbackStatus = PlaybackStatus.stopped;
    notifyListeners();
    _scheduleReconnect();
  }

  void _scheduleReconnect() {
    if (!_autoReconnect) return;
    final idx = _reconnectAttempts.clamp(0, _reconnectDelays.length - 1);
    final delay = _reconnectDelays[idx];
    _reconnectAttempts++;
    LogService.info('NowPlayingServerListener: reconnecting in ${delay}s');
    _reconnectTimer?.cancel();
    _reconnectTimer = Timer(Duration(seconds: delay), _connect);
  }

  void _closeCurrentConnection() {
    _subscription?.cancel();
    _subscription = null;
    _stopPositionTimer();
    final ch = _channel;
    _channel = null;
    try {
      ch?.sink.close();
    } catch (_) {}
  }

  PlaybackStatus _parseState(String state) => switch (state) {
    'playing' => PlaybackStatus.playing,
    'paused' => PlaybackStatus.paused,
    _ => PlaybackStatus.stopped,
  };

  @override
  void dispose() {
    _autoReconnect = false;
    _reconnectTimer?.cancel();
    _reconnectTimer = null;
    _closeCurrentConnection();
    super.dispose();
  }
}
