import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:modern_gauge_flutter/models/media_info.dart';
import 'package:modern_gauge_flutter/providers/mpris_provider.dart';
import 'package:modern_gauge_flutter/services/log_service.dart';

/// Source « now playing » factice pour déboguer l'écran musique sans serveur
/// Go. Simule un morceau en lecture dont la position avance en temps réel,
/// puis enchaîne sur le suivant.
///
/// Activation : `flutter run --dart-define=FAKE_NOW_PLAYING=true`
class FakeNowPlayingListener
    with ChangeNotifier
    implements MprisListenerBase {
  static final _tracks = [
    MediaInfo(
      title: 'Father Figure (Remastered)',
      artist: 'George Michael',
      album: 'Faith',
      duration: const Duration(minutes: 5, seconds: 41),
    ),
    MediaInfo(
      title: 'Poison Ivy',
      artist: 'Tory Lanez',
      album: 'Alone at Prom (Deluxe)',
      duration: const Duration(minutes: 3, seconds: 8),
    ),
  ];

  Timer? _timer;
  int _trackIndex = 0;
  Duration _position = const Duration(minutes: 3, seconds: 7);

  @override
  MediaInfo? get mediaInfo => _tracks[_trackIndex];

  @override
  Duration get position => _position;

  @override
  PlaybackStatus get playbackStatus => PlaybackStatus.playing;

  @override
  bool get isPlaying => true;

  @override
  Future<void> start() async {
    LogService.info('FakeNowPlayingListener: source factice active');
    _timer = Timer.periodic(const Duration(milliseconds: 500), (_) {
      _position += const Duration(milliseconds: 500);
      if (_position >= mediaInfo!.duration!) {
        _trackIndex = (_trackIndex + 1) % _tracks.length;
        _position = Duration.zero;
      }
      notifyListeners();
    });
  }

  @override
  void dispose() {
    _timer?.cancel();
    super.dispose();
  }
}
