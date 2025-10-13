import 'dart:async';
import 'dart:developer';
import 'dart:io';
import 'package:flutter/foundation.dart';
import 'package:dbus/dbus.dart';
import 'package:modern_gauge_flutter/providers/mpris_provider.dart';

class MediaInfo {
  final String? title;
  final String? artist;
  final String? album;
  final String? artUrl;
  final Duration? duration;

  MediaInfo({this.title, this.artist, this.album, this.artUrl, this.duration});
}

enum PlaybackStatus { playing, paused, stopped }

class MprisListener implements MprisListenerBase {
  static final _mprisPath = DBusObjectPath('/org/mpris/MediaPlayer2');
  static const _playerInterface = 'org.mpris.MediaPlayer2.Player';
  static const _mprisPrefix = 'org.mpris.MediaPlayer2.';

  // Position sync config
  static const _positionSyncInterval = Duration(milliseconds: 500);
  static const _positionUpdateDebounce = Duration(milliseconds: 100);

  late final DBusClient _client;
  final ChangeNotifier _changeNotifier = ChangeNotifier();

  DBusRemoteObject? _playerObject;
  String? _currentPlayerName;

  StreamSubscription? _propertiesSubscription;
  StreamSubscription<DBusNameOwnerChangedEvent>? _nameOwnerChangedSubscription;
  Timer? _positionSyncTimer;
  Timer? _positionUpdateTimer;

  MediaInfo? _mediaInfo;
  Duration _position = Duration.zero;
  Duration _lastSyncedPosition = Duration.zero;
  PlaybackStatus _playbackStatus = PlaybackStatus.stopped;
  DateTime? _lastStatusChangeTime;

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
      log(
        'MprisListener INFO: Skipping initialization. Not on Linux platform.',
      );
      return;
    }

    try {
      _client = DBusClient.session();
      _listenForPlayerChanges();
      await _scanForInitialPlayer();
    } catch (e) {
      log("MprisListener ERREUR: Impossible de se connecter à D-Bus: $e");
    }
  }

  void _listenForPlayerChanges() {
    _nameOwnerChangedSubscription = _client.nameOwnerChanged.listen(
      (signal) {
        if (!signal.name.startsWith(_mprisPrefix)) return;

        final hasNewOwner =
            signal.newOwner != null && signal.newOwner!.isNotEmpty;
        final hasOldOwner =
            signal.oldOwner != null && signal.oldOwner!.isNotEmpty;

        if (hasNewOwner && _playerObject == null) {
          _connectToPlayer(signal.name);
        } else if (!hasNewOwner &&
            hasOldOwner &&
            signal.name == _currentPlayerName) {
          _disconnectAndReset();
        }
      },
      onError: (e) {
        log("ERREUR nameOwnerChanged: $e");
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
      log("ERREUR lors du scan initial: $e");
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
      _startPositionSyncTimer();
      notifyListeners();
    } catch (e) {
      log("ERREUR lors de la connexion: $e");
      await _disconnectAndReset();
    }
  }

  Future<void> _fetchInitialState() async {
    if (_playerObject == null) return;

    try {
      final properties = await _playerObject!.getAllProperties(
        _playerInterface,
      );

      _updateMetadata(properties['Metadata']?.asStringVariantDict() ?? {});
      _updatePlaybackStatus(
        properties['PlaybackStatus']?.asString() ?? 'Stopped',
      );

      await _syncPosition();
    } catch (e) {
      log("ERREUR fetch initial state: $e");
      rethrow;
    }
  }

  void _listenToPlayerSignals() {
    if (_playerObject == null) return;

    _propertiesSubscription?.cancel();
    _propertiesSubscription = _playerObject!.propertiesChanged.listen(
      (signal) {
        final props = signal.changedProperties;

        // Handle metadata changes
        if (props.containsKey('Metadata')) {
          _updateMetadata(props['Metadata']!.asStringVariantDict());
        }

        // Handle playback status changes
        final newStatusString = props['PlaybackStatus']?.asString();
        if (newStatusString != null) {
          _updatePlaybackStatus(newStatusString);
        }

        // Handle direct position updates (less common)
        if (props.containsKey('Position')) {
          _updatePosition(props['Position']!.asInt64());
        }

        notifyListeners();
      },
      onError: (e) {
        log("ERREUR propertiesChanged: $e");
        _disconnectAndReset();
      },
    );
  }

  Future<void> _syncPosition() async {
    if (_playerObject == null) return;

    try {
      final positionVariant = await _playerObject!.getProperty(
        _playerInterface,
        'Position',
      );

      final newPosition = Duration(microseconds: positionVariant.asInt64());
      _updatePosition(positionVariant.asInt64());
      _lastSyncedPosition = newPosition;
    } catch (e) {
      log("ERREUR sync position: $e");
    }
  }

  void _startPositionSyncTimer() {
    _positionSyncTimer?.cancel();
    _positionSyncTimer = Timer.periodic(_positionSyncInterval, (_) async {
      if (isPlaying) {
        await _syncPosition();
        notifyListeners();
      }
    });
  }

  void _updatePlaybackStatus(String status) {
    final newStatus = switch (status) {
      'Playing' => PlaybackStatus.playing,
      'Paused' => PlaybackStatus.paused,
      _ => PlaybackStatus.stopped,
    };

    if (_playbackStatus != newStatus) {
      _playbackStatus = newStatus;
      _lastStatusChangeTime = DateTime.now();

      log("Playback status: ${_playbackStatus.name}");

      // Start/stop position sync based on playback state
      if (isPlaying) {
        _startPositionSyncTimer();
      } else {
        _positionSyncTimer?.cancel();
      }
    }
  }

  Future<void> _disconnectAndReset({bool notify = true}) async {
    try {
      await _propertiesSubscription?.cancel();
      _propertiesSubscription = null;

      _positionSyncTimer?.cancel();
      _positionSyncTimer = null;

      _positionUpdateTimer?.cancel();
      _positionUpdateTimer = null;

      _playerObject = null;
      _currentPlayerName = null;
      _mediaInfo = null;
      _position = Duration.zero;
      _lastSyncedPosition = Duration.zero;
      _playbackStatus = PlaybackStatus.stopped;
      _lastStatusChangeTime = null;

      if (notify) {
        notifyListeners();
      }
    } catch (e) {
      log("ERREUR disconnectAndReset: $e");
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

      log("Metadata updated: ${_mediaInfo?.title} - ${_mediaInfo?.artist}");
    } catch (e) {
      log("ERREUR updateMetadata: $e");
    }
  }

  void _updatePosition(int microseconds) {
    _position = Duration(microseconds: microseconds);
  }

  @override
  void addListener(VoidCallback listener) =>
      _changeNotifier.addListener(listener);

  @override
  void removeListener(VoidCallback listener) =>
      _changeNotifier.removeListener(listener);

  @override
  void notifyListeners() => _changeNotifier.notifyListeners();

  @override
  bool get hasListeners => _changeNotifier.hasListeners;

  @override
  Future<void> dispose() async {
    try {
      await _nameOwnerChangedSubscription?.cancel();
      await _disconnectAndReset(notify: false);
      _changeNotifier.dispose();
      await _client.close();
    } catch (e) {
      log("ERREUR dispose: $e");
    }
  }
}
