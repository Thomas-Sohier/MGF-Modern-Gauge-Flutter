/// Now-playing data model shared by the now-playing listener(s) and UI.
class MediaInfo {
  final String? title;
  final String? artist;
  final String? album;
  final String? artUrl;
  final Duration? duration;

  MediaInfo({this.title, this.artist, this.album, this.artUrl, this.duration});
}

enum PlaybackStatus { playing, paused, stopped }
