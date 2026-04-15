import 'dart:async';
import 'dart:io';
import 'package:flutter/foundation.dart';
import 'package:dbus/dbus.dart';
import 'package:modern_gauge_flutter/providers/mpris_provider.dart';
import 'package:modern_gauge_flutter/services/log_service.dart';

class MediaInfo {
  final String? title;
  final String? artist;
  final String? album;
  final String? artUrl;
  final Duration? duration;

  MediaInfo({this.title, this.artist, this.album, this.artUrl, this.duration});
}

enum PlaybackStatus { playing, paused, stopped }

class MprisListener with ChangeNotifier implements MprisListenerBase {
  static final _mprisPath = DBusObjectPath('/org/mpris/MediaPlayer2');
  static const _playerInterface = 'org.mpris.MediaPlayer2.Player';
  static const _mprisPrefix = 'org.mpris.MediaPlayer2.';

  static const _positionUpdateInterval = Duration(milliseconds: 100);

  late final DBusClient _client;

  DBusRemoteObject? _playerObject;
  String? _currentPlayerName;

  StreamSubscription? _propertiesSubscription;
  StreamSubscription<DBusNameOwnerChangedEvent>? _nameOwnerChangedSubscription;

  Timer? _positionUpdateTimer;

  MediaInfo? _mediaInfo;
  Duration _position = Duration.zero;
  PlaybackStatus _playbackStatus = PlaybackStatus.stopped;

  @override
  MediaInfo? get mediaInfo => _mediaInfo;

  @override
  Duration get position => _position;

  @override
  PlaybackStatus get playbackStatus => _playbackStatus;

  @override
  bool get isPlaying => _playbackStatus == PlaybackStatus.playing;

  MprisListener();

  @override
  Future<void> start() async {
    if (!Platform.isLinux) {
      LogService.warning('[MprisListener] - Skipping initialization. Not on Linux platform.');
      return;
    }

    try {
      _client = DBusClient.session();
      _listenForPlayerChanges();
      await _scanForInitialPlayer();
    } catch (e) {
      LogService.warning("[MprisListener] - Cannot connect to D-Bus: $e");
    }
  }

  void _listenForPlayerChanges() {
    _nameOwnerChangedSubscription = _client.nameOwnerChanged.listen(
      (signal) {
        if (!signal.name.startsWith(_mprisPrefix)) return;

        final hasNewOwner = signal.newOwner != null && signal.newOwner!.isNotEmpty;
        final hasOldOwner = signal.oldOwner != null && signal.oldOwner!.isNotEmpty;

        if (hasNewOwner && _playerObject == null) {
          _connectToPlayer(signal.name);
        } else if (!hasNewOwner && hasOldOwner && signal.name == _currentPlayerName) {
          _disconnectAndReset();
        }
      },
      onError: (e) {
        LogService.error("[MprisListener] - nameOwnerChanged: $e");
        _disconnectAndReset();
      },
    );
  }

  Future<void> _scanForInitialPlayer() async {
    try {
      final names = await _client.listNames();
      final playerOwner = names.firstWhere(
        (name) => name.startsWith(_mprisPrefix),
        orElse: () => '',
      );
      if (playerOwner.isNotEmpty) {
        await _connectToPlayer(playerOwner);
      }
    } catch (e) {
      LogService.error("[MprisListener] - error while initial scan : $e");
    }
  }

  Future<void> _connectToPlayer(String name) async {
    if (_currentPlayerName == name) return;

    await _disconnectAndReset(notify: false);
    _currentPlayerName = name;
    _playerObject = DBusRemoteObject(_client, name: name, path: _mprisPath);

    try {
      await _fetchInitialState();
      _listenToPlayerSignals();
      notifyListeners();
    } catch (e) {
      LogService.error("[MprisListener] - error while connect : $e");
      await _disconnectAndReset();
    }
  }

  Future<void> _fetchInitialState() async {
    if (_playerObject == null) return;

    try {
      final properties = await _playerObject!.getAllProperties(_playerInterface);

      _updateMetadata(properties['Metadata']?.asStringVariantDict() ?? {});

      final statusString = properties['PlaybackStatus']?.asString() ?? 'Stopped';
      _playbackStatus = _stringToPlaybackStatus(statusString);
      LogService.info("[MprisListener] - Initial playback status: ${_playbackStatus.name}");

      await _syncPosition();

      if (isPlaying) {
        _startPositionUpdateTimer();
      }
    } catch (e) {
      LogService.error("[MprisListener] - fetch initial state: $e");
      rethrow;
    }
  }

  void _listenToPlayerSignals() {
    if (_playerObject == null) return;

    _propertiesSubscription?.cancel();
    _propertiesSubscription = _playerObject!.propertiesChanged.listen(
      (signal) async {
        final props = signal.changedProperties;
        LogService.debug("[MprisListener] - propertiesChanged: $props");
        bool needsNotify = false;

        if (props.containsKey('Metadata')) {
          _updateMetadata(props['Metadata']!.asStringVariantDict());
          await _syncPosition();
          needsNotify = true;
        }

        if (props.containsKey('Position')) {
          _updatePosition(props['Position']!.asInt64());
          needsNotify = true;
        }

        final newStatusString = props['PlaybackStatus']?.asString();
        if (newStatusString != null) {
          final newStatus = _stringToPlaybackStatus(newStatusString);
          if (_playbackStatus != newStatus) {
            _playbackStatus = newStatus;
            LogService.info("[MprisListener] - Playback status: ${_playbackStatus.name}");

            if (isPlaying) {
              await _syncPosition();
              _startPositionUpdateTimer();
            } else {
              _stopPositionUpdateTimer();
              await _syncPosition();
            }
            needsNotify = true;
          }
        }

        if (needsNotify) {
          notifyListeners();
        }
      },
      onError: (e) {
        LogService.error("[MprisListener] - propertiesChanged: $e");
        _disconnectAndReset();
      },
    );
  }

  Future<void> _syncPosition() async {
    if (_playerObject == null) return;
    try {
      final positionVariant = await _playerObject!.getProperty(_playerInterface, 'Position');
      _updatePosition(positionVariant.asInt64());
    } catch (e) {
      LogService.error("[MprisListener] - sync position: $e");
    }
  }

  void _startPositionUpdateTimer() {
    _stopPositionUpdateTimer();
    _positionUpdateTimer = Timer.periodic(_positionUpdateInterval, (_) {
      _position += _positionUpdateInterval;
      final duration = _mediaInfo?.duration ?? Duration.zero;
      if (duration > Duration.zero && _position > duration) {
        _position = duration;
        _stopPositionUpdateTimer();
      }
      notifyListeners();
    });
  }

  void _stopPositionUpdateTimer() {
    _positionUpdateTimer?.cancel();
    _positionUpdateTimer = null;
  }

  PlaybackStatus _stringToPlaybackStatus(String status) {
    return switch (status) {
      'Playing' => PlaybackStatus.playing,
      'Paused' => PlaybackStatus.paused,
      _ => PlaybackStatus.stopped,
    };
  }

  Future<void> _disconnectAndReset({bool notify = true}) async {
    try {
      await _propertiesSubscription?.cancel();
      _propertiesSubscription = null;

      _stopPositionUpdateTimer();

      _playerObject = null;
      _currentPlayerName = null;
      _mediaInfo = null;
      _position = Duration.zero;
      _playbackStatus = PlaybackStatus.stopped;
      if (notify) {
        notifyListeners();
      }
    } catch (e) {
      LogService.error("[MprisListener] - disconnectAndReset: $e");
    }
  }

  void _updateMetadata(Map<String, DBusValue> metadata) {
    try {
      final artUrl = metadata['mpris:artUrl']?.asString();
      final durationInMicroseconds = metadata['mpris:length']?.asInt64() ?? 0;

      _mediaInfo = MediaInfo(
        title: metadata['xesam:title']?.asString(),
        artist: metadata['xesam:artist']?.asStringArray().join(', '),
        album: metadata['xesam:album']?.asString(),
        artUrl: artUrl?.startsWith('file://') ?? false
            ? Uri.decodeComponent(artUrl!.substring(7))
            : artUrl,
        duration: Duration(microseconds: durationInMicroseconds),
      );

      LogService.info("[MprisListener] - updated: ${_mediaInfo?.title} - ${_mediaInfo?.artist}");
    } catch (e) {
      LogService.error("[MprisListener] - updateMetadata: $e");
    }
  }

  void _updatePosition(int microseconds) {
    _position = Duration(microseconds: microseconds);
  }

  @override
  Future<void> dispose() async {
    try {
      await _nameOwnerChangedSubscription?.cancel();
      await _disconnectAndReset(notify: false);
      super.dispose();
      await _client.close();
    } catch (e) {
      LogService.debug("[MprisListener] - dispose: $e");
    }
  }
}
