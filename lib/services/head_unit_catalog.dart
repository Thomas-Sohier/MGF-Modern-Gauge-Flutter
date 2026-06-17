import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/routes/route_names.dart';

/// A dashboard view exposed to the companion phone: a route segment (its stable
/// id) and a human label. The ordered list mirrors the dashboard rotation.
class HeadUnitView {
  const HeadUnitView(this.segment, this.label);

  /// Route segment, e.g. `/rpm`. Used as the catalog `view_id` and as the
  /// key in [SettingsData.enabledScreens].
  final String segment;
  final String label;

  /// Full router path for this view.
  String get fullPath => '${RouteNames.dashboardRoute}$segment';
}

/// Setting ids exposed for remote editing.
class HeadUnitSettingIds {
  static const String theme = 'theme';
  static const String wifi = 'wifi';
  static const String notificationDuration = 'notification_duration';
}

/// Ordered dashboard views, by route segment, with display labels. Mirrors the
/// rotation order in `navigation_logic.dart`; settings (`/settings`) and splash
/// are intentionally excluded — they are not part of the switchable rotation.
const List<HeadUnitView> headUnitViews = [
  HeadUnitView('/time', 'Horloge'),
  HeadUnitView('/music', 'Musique'),
  HeadUnitView('/navigation', 'Navigation'),
  HeadUnitView('/rpm', 'Régime'),
  HeadUnitView('/faults', 'Défauts'),
  HeadUnitView('/temps', 'Températures'),
  HeadUnitView('/injection', 'Injection'),
  HeadUnitView('/lambda', 'Lambda'),
  HeadUnitView('/allumage', 'Allumage'),
  HeadUnitView('/ralenti', 'Ralenti'),
  HeadUnitView('/admission', 'Admission'),
];

/// Wire value for a [ThemeMode] in the catalog `theme` enum setting.
String themeModeToWire(ThemeMode mode) => switch (mode) {
  ThemeMode.light => 'light',
  ThemeMode.dark => 'dark',
  ThemeMode.system => 'system',
};

/// Parses a catalog `theme` enum value, or null if unrecognised.
ThemeMode? themeModeFromWire(String value) => switch (value) {
  'light' => ThemeMode.light,
  'dark' => ThemeMode.dark,
  'system' => ThemeMode.system,
  _ => null,
};

/// Builds the self-describing catalog the head unit publishes to the agent.
///
/// [currentLocation] is the active router path; [enabledScreens] are the
/// visible route segments. A view is `current` only when the active path is
/// exactly its full path (so on `/settings` or splash, no view is current).
Map<String, dynamic> buildHeadUnitCatalog({
  required String currentLocation,
  required Set<String> enabledScreens,
  required ThemeMode themeMode,
  required bool wifiEnabled,
  required int notificationDurationSeconds,
  required int minNotificationDurationSeconds,
  required int maxNotificationDurationSeconds,
}) {
  return {
    'views': [
      for (final v in headUnitViews)
        {
          'id': v.segment,
          'label': v.label,
          'visible': enabledScreens.contains(v.segment),
          'current': currentLocation == v.fullPath,
        },
    ],
    'settings': [
      {
        'id': HeadUnitSettingIds.theme,
        'label': 'Thème',
        'type': 'enum',
        'value': themeModeToWire(themeMode),
        'options': const [
          {'value': 'system', 'label': 'Système'},
          {'value': 'light', 'label': 'Clair'},
          {'value': 'dark', 'label': 'Sombre'},
        ],
      },
      {
        'id': HeadUnitSettingIds.wifi,
        'label': 'WiFi',
        'type': 'bool',
        'value': wifiEnabled,
      },
      {
        'id': HeadUnitSettingIds.notificationDuration,
        'label': 'Durée notifications',
        'type': 'number',
        'value': notificationDurationSeconds,
        'min': minNotificationDurationSeconds,
        'max': maxNotificationDurationSeconds,
        'step': 1,
      },
    ],
  };
}
