import 'dart:async';
import 'dart:developer';
import 'dart:io';
import 'package:flutter/foundation.dart';
import 'package:dbus/dbus.dart';
import 'package:modern_gauge_flutter/providers/mpris_provider.dart';

// Classe pour stocker les informations du média
class MediaInfo {
  final String? title;
  final String? artist;
  final String? album;
  final String? artUrl;
  final Duration? duration;

  MediaInfo({this.title, this.artist, this.album, this.artUrl, this.duration});
}

// Le service qui écoute MPRIS
class MprisListener implements MprisListenerBase {
  static final mprisPath = DBusObjectPath('/org/mpris/MediaPlayer2');
  static const playerInterface = 'org.mpris.MediaPlayer2.Player';
  static const propertiesInterface = 'org.freedesktop.DBus.Properties';

  late final DBusClient _client;
  final ChangeNotifier _changeNotifier = ChangeNotifier();
  DBusRemoteObject? _playerObject;
  StreamSubscription? _propertiesSubscription;
  Timer? _positionTimer;

  // --- Données pour l'UI ---
  MediaInfo? _mediaInfo;

  @override
  MediaInfo? get mediaInfo => _mediaInfo;

  Duration _position = Duration.zero;
  @override
  Duration get position => _position;

  String _playbackStatus = 'Stopped';
  @override
  String get playbackStatus => _playbackStatus;

  @override
  bool get isPlaying => _playbackStatus == 'Playing';

  MprisListener();

  // Démarre l'écoute
  @override
  Future<void> start() async {
    if (!Platform.isLinux) {
      log('MprisListener INFO: Skipping initialization. Not on Linux platform.');
      return;
    }

    _client = DBusClient.session();

    final names = await _client.listNames();
    final playerOwner = names.firstWhere((name) => name.startsWith('org.mpris.MediaPlayer2.'), orElse: () => '');

    if (playerOwner.isEmpty) {
      log("Aucun lecteur MPRIS trouvé.");
      return;
    }

    log("Lecteur trouvé : $playerOwner");

    _playerObject = DBusRemoteObject(_client, name: playerOwner, path: mprisPath);
    await _fetchInitialState();
    _listenToChanges();
  }

  // Récupère toutes les propriétés au démarrage
  Future<void> _fetchInitialState() async {
    if (_playerObject == null) return;

    final metadataVariant = await _playerObject!.getProperty(playerInterface, 'Metadata');
    _updateMetadata(metadataVariant.asStringVariantDict());
    final statusVariant = await _playerObject!.getProperty(playerInterface, 'PlaybackStatus');
    _updatePlaybackStatus(statusVariant.asString());

    try {
      final positionVariant = await _playerObject!.getProperty(playerInterface, 'Position');
      _updatePosition(positionVariant.asInt64());
    } catch (e) {
      log("Impossible de récupérer la position initiale: $e");
      _position = Duration.zero;
    }

    notifyListeners();
  }

  // S'abonne aux signaux de changement
  void _listenToChanges() {
    _propertiesSubscription?.cancel();
    _propertiesSubscription = _playerObject?.propertiesChanged.listen((signal) {
      final changedProps = signal.changedProperties;

      if (changedProps.containsKey('Metadata')) {
        _updateMetadata(changedProps['Metadata']!.asStringVariantDict());
      }
      if (changedProps.containsKey('PlaybackStatus')) {
        _updatePlaybackStatus(changedProps['PlaybackStatus']!.asString());
      }
      notifyListeners();
    });
  }

  void _updateMetadata(Map<String, DBusValue> metadata) {
    final artUrl = metadata['mpris:artUrl']?.asString();
    final durationInMicroseconds = metadata['mpris:length']?.asInt64() ?? 0;

    _mediaInfo = MediaInfo(
      title: metadata['xesam:title']?.asString(),
      artist: metadata['xesam:artist']?.asStringArray().join(', '),
      album: metadata['xesam:album']?.asString(),
      artUrl: artUrl?.startsWith('file://') ?? false ? Uri.decodeComponent(artUrl!) : artUrl,
      duration: Duration(microseconds: durationInMicroseconds),
    );
  }

  void _updatePlaybackStatus(String status) {
    _playbackStatus = status;
    log("Nouveau statut : $_playbackStatus");
    if (isPlaying) {
      _startPositionTimer();
    } else {
      _positionTimer?.cancel();
    }
  }

  void _updatePosition(int microseconds) {
    _position = Duration(microseconds: microseconds);
  }

  // Le signal DBus pour la position n'est pas envoyé en continu.
  // On utilise un timer pour simuler une progression fluide.
  void _startPositionTimer() {
    _positionTimer?.cancel();
    _positionTimer = Timer.periodic(const Duration(seconds: 1), (timer) {
      if (isPlaying && (_mediaInfo?.duration ?? Duration.zero) > Duration.zero) {
        _position += const Duration(seconds: 1);
        if (_position > _mediaInfo!.duration!) {
          _position = _mediaInfo!.duration!;
        }
        notifyListeners();
      }
    });
  }

  @override
  void addListener(VoidCallback listener) {
    _changeNotifier.addListener(listener);
  }

  @override
  void removeListener(VoidCallback listener) {
    _changeNotifier.removeListener(listener);
  }

  @override
  void dispose() {
    _changeNotifier.dispose();
    _propertiesSubscription?.cancel();
    _positionTimer?.cancel();
    _client.close();
  }

  @override
  void notifyListeners() {
    _changeNotifier.notifyListeners();
  }

  @override
  bool get hasListeners => _changeNotifier.hasListeners;
}
