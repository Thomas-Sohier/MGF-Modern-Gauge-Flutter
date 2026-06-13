import 'package:flutter/foundation.dart';
import 'package:modern_gauge_flutter/models/media_info.dart';

abstract class MprisListenerBase with ChangeNotifier {
  MediaInfo? get mediaInfo;
  Duration get position;
  PlaybackStatus get playbackStatus;
  bool get isPlaying;

  Future<void> start();
}
