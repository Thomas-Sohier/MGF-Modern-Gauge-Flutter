import 'package:flutter/foundation.dart';
import 'package:modern_gauge_flutter/services/mpris_listener.dart';

abstract class MprisListenerBase with ChangeNotifier {
  MediaInfo? get mediaInfo;
  Duration get position;
  PlaybackStatus get playbackStatus;
  bool get isPlaying;

  Future<void> start();
}
