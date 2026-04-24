import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:modern_gauge_flutter/models/settings_data.dart';

void main() {
  group('SettingsProvider logic', () {
    late TestableSettingsProvider provider;

    setUp(() {
      provider = TestableSettingsProvider();
    });

    group('toggleScreen', () {
      test('removes screen when present', () {
        provider.setSettings(SettingsData(enabledScreens: {'/rpm', '/time'}));

        provider.toggleScreen('/rpm');

        expect(provider.settings.enabledScreens, equals({'/time'}));
      });

      test('adds screen when absent', () {
        provider.setSettings(SettingsData(enabledScreens: {'/time'}));

        provider.toggleScreen('/rpm');

        expect(provider.settings.enabledScreens, contains('/rpm'));
        expect(provider.settings.enabledScreens, contains('/time'));
      });

      test('notifies listeners on toggle', () {
        provider.setSettings(SettingsData(enabledScreens: {'/rpm'}));

        var notified = false;
        provider.addListener(() => notified = true);

        provider.toggleScreen('/rpm');

        expect(notified, isTrue);
      });

      test('handles empty set', () {
        provider.setSettings(SettingsData(enabledScreens: {}));

        provider.toggleScreen('/music');

        expect(provider.settings.enabledScreens, equals({'/music'}));
      });

      test('preserves other screens when toggling', () {
        provider.setSettings(
          SettingsData(enabledScreens: {'/rpm', '/time', '/music', '/faults'}),
        );

        provider.toggleScreen('/time');

        expect(provider.settings.enabledScreens, equals({'/rpm', '/music', '/faults'}));
      });
    });

    group('setBackgroundImage', () {
      test('updates background path', () {
        provider.setBackgroundImage('/path/to/image.png');

        expect(provider.settings.backgroundImagePath, equals('/path/to/image.png'));
      });


      test('notifies listeners', () {
        var notified = false;
        provider.addListener(() => notified = true);

        provider.setBackgroundImage('/new/path.png');

        expect(notified, isTrue);
      });
    });

    group('setThemeMode', () {
      test('updates theme mode to light', () {
        provider.setThemeMode(ThemeMode.light);

        expect(provider.settings.themeMode, equals(ThemeMode.light));
      });

      test('updates theme mode to dark', () {
        provider.setSettings(SettingsData(themeMode: ThemeMode.light));

        provider.setThemeMode(ThemeMode.dark);

        expect(provider.settings.themeMode, equals(ThemeMode.dark));
      });

      test('updates theme mode to system', () {
        provider.setThemeMode(ThemeMode.system);

        expect(provider.settings.themeMode, equals(ThemeMode.system));
      });

      test('notifies listeners', () {
        var notified = false;
        provider.addListener(() => notified = true);

        provider.setThemeMode(ThemeMode.light);

        expect(notified, isTrue);
      });
    });

    group('updateSettings', () {
      test('replaces entire settings object', () {
        final newSettings = SettingsData(
          backgroundImagePath: '/bg.png',
          themeMode: ThemeMode.light,
          enabledScreens: {'/rpm'},
        );

        provider.updateSettings(newSettings);

        expect(provider.settings.backgroundImagePath, equals('/bg.png'));
        expect(provider.settings.themeMode, equals(ThemeMode.light));
        expect(provider.settings.enabledScreens, equals({'/rpm'}));
      });

      test('notifies listeners', () {
        var notified = false;
        provider.addListener(() => notified = true);

        provider.updateSettings(SettingsData());

        expect(notified, isTrue);
      });
    });

    group('resetSettings', () {
      test('resets to default values', () {
        provider.setSettings(SettingsData(
          backgroundImagePath: '/custom.png',
          themeMode: ThemeMode.light,
          enabledScreens: {'/rpm'},
        ));

        provider.resetSettings();

        expect(provider.settings.backgroundImagePath, isNull);
        expect(provider.settings.themeMode, equals(ThemeMode.dark));
        expect(provider.settings.enabledScreens, equals(SettingsData.allScreens));
      });

      test('notifies listeners', () {
        var notified = false;
        provider.addListener(() => notified = true);

        provider.resetSettings();

        expect(notified, isTrue);
      });
    });

    group('notification count', () {
      test('notifies exactly once per update', () {
        var count = 0;
        provider.addListener(() => count++);

        provider.toggleScreen('/rpm');
        provider.setBackgroundImage('/path.png');
        provider.setThemeMode(ThemeMode.light);

        expect(count, equals(3));
      });
    });
  });
}

/// Testable SettingsProvider that doesn't depend on SettingsService singleton.
class TestableSettingsProvider with ChangeNotifier {
  SettingsData _settings = SettingsData();

  SettingsData get settings => _settings;

  void setSettings(SettingsData s) {
    _settings = s;
  }

  void _update(SettingsData s) {
    _settings = s;
    notifyListeners();
  }

  void updateSettings(SettingsData newSettings) => _update(newSettings);

  void setBackgroundImage(String? path) =>
      _update(_settings.copyWith(backgroundImagePath: path));

  void setThemeMode(ThemeMode mode) =>
      _update(_settings.copyWith(themeMode: mode));

  void toggleScreen(String routeSegment) {
    final current = Set<String>.from(_settings.enabledScreens);
    if (current.contains(routeSegment)) {
      current.remove(routeSegment);
    } else {
      current.add(routeSegment);
    }
    _update(_settings.copyWith(enabledScreens: current));
  }

  void resetSettings() => _update(SettingsData());
}
