import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:modern_gauge_flutter/services/head_unit_catalog.dart';

void main() {
  group('buildHeadUnitCatalog', () {
    Map<String, dynamic> build({
      String currentLocation = '/dashboard/rpm',
      Set<String> enabledScreens = const {'/rpm', '/time'},
      ThemeMode themeMode = ThemeMode.dark,
      bool wifiEnabled = true,
      int notificationDurationSeconds = 10,
    }) => buildHeadUnitCatalog(
      currentLocation: currentLocation,
      enabledScreens: enabledScreens,
      themeMode: themeMode,
      wifiEnabled: wifiEnabled,
      notificationDurationSeconds: notificationDurationSeconds,
      minNotificationDurationSeconds: 3,
      maxNotificationDurationSeconds: 30,
    );

    test('marks the active path current and reflects visibility', () {
      final views = build()['views'] as List;
      final rpm = views.firstWhere((v) => v['id'] == '/rpm');
      final music = views.firstWhere((v) => v['id'] == '/music');
      expect(rpm['current'], isTrue);
      expect(rpm['visible'], isTrue);
      expect(music['current'], isFalse);
      expect(music['visible'], isFalse);
    });

    test('no view is current outside the dashboard rotation', () {
      final views = build(currentLocation: '/settings')['views'] as List;
      expect(views.any((v) => v['current'] == true), isFalse);
    });

    test('exposes one setting of each supported type', () {
      final settings = build()['settings'] as List;
      final byId = {for (final s in settings) s['id']: s};
      expect(byId['theme']!['type'], 'enum');
      expect(byId['theme']!['value'], 'dark');
      expect((byId['theme']!['options'] as List).map((o) => o['value']),
          containsAll(['system', 'light', 'dark']));
      expect(byId['wifi']!['type'], 'bool');
      expect(byId['wifi']!['value'], true);
      final dur = byId['notification_duration']!;
      expect(dur['type'], 'number');
      expect(dur['value'], 10);
      expect(dur['min'], 3);
      expect(dur['max'], 30);
    });
  });

  group('theme wire mapping', () {
    test('round-trips every mode', () {
      for (final mode in ThemeMode.values) {
        expect(themeModeFromWire(themeModeToWire(mode)), mode);
      }
    });

    test('returns null for an unknown value', () {
      expect(themeModeFromWire('sepia'), isNull);
    });
  });
}
