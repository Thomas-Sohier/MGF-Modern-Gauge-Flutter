/// A one-shot notification forwarded from the phone via the Go server's
/// /ws/notifications websocket. Alerts are fire-once events (a message, an
/// email); the server never sends updates or dismissals for them.
class AlertInfo {
  /// Human-readable app label the notification came from (e.g. "Signal").
  final String app;
  final String title;
  final String text;

  /// When the notification was posted on the phone.
  final DateTime postedAt;

  const AlertInfo({
    required this.app,
    required this.title,
    required this.text,
    required this.postedAt,
  });

  /// Builds an [AlertInfo] from a /ws/notifications message map.
  factory AlertInfo.fromJson(Map<String, dynamic> json) {
    final postedAtMs = json['posted_at'] as int? ?? 0;
    return AlertInfo(
      app: json['app'] as String? ?? '',
      title: json['title'] as String? ?? '',
      text: json['text'] as String? ?? '',
      postedAt: DateTime.fromMillisecondsSinceEpoch(postedAtMs),
    );
  }
}
