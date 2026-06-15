/// Turn-by-turn navigation state pushed by the Go server's /ws/navigation
/// websocket. A single, continuously-replacing instruction (not a list).
///
/// Google Maps only exposes localized display strings plus a rendered maneuver
/// arrow, so [instruction], [distance] and [eta] are human-readable text and
/// [iconUrl] points at an image — there is no structured maneuver data.
class NavigationInfo {
  /// Whether navigation is currently active. When false the other fields are
  /// null and the UI should clear any navigation display.
  final bool active;
  final String? instruction;
  final String? distance;
  final String? eta;

  /// URL of the maneuver-icon PNG, or null when no icon is available. The
  /// icon id is appended as a query parameter so the image cache invalidates
  /// whenever the maneuver changes.
  final String? iconUrl;

  const NavigationInfo({
    required this.active,
    this.instruction,
    this.distance,
    this.eta,
    this.iconUrl,
  });

  /// Empty (inactive) navigation state.
  const NavigationInfo.inactive()
    : active = false,
      instruction = null,
      distance = null,
      eta = null,
      iconUrl = null;

  /// Builds a [NavigationInfo] from a /ws/navigation snapshot map. [iconBaseUrl]
  /// is the GET /api/navigation/icon endpoint used to fetch the maneuver PNG.
  factory NavigationInfo.fromSnapshot(
    Map<String, dynamic> json, {
    required String iconBaseUrl,
  }) {
    final nav = json['navigation'] as Map<String, dynamic>? ?? const {};
    final hasIcon = json['has_icon'] as bool? ?? false;
    final iconId = json['icon_id'] as String? ?? '';

    String? nonEmpty(Object? v) {
      final s = v as String?;
      return (s != null && s.isNotEmpty) ? s : null;
    }

    final iconUrl = (hasIcon && iconId.isNotEmpty)
        ? '$iconBaseUrl?iconId=${Uri.encodeComponent(iconId)}'
        : null;

    return NavigationInfo(
      active: nav['active'] as bool? ?? false,
      instruction: nonEmpty(nav['instruction']),
      distance: nonEmpty(nav['distance']),
      eta: nonEmpty(nav['eta']),
      iconUrl: iconUrl,
    );
  }
}
