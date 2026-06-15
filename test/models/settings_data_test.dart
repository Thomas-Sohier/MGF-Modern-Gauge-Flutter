import 'package:flutter_test/flutter_test.dart';
import 'package:modern_gauge_flutter/models/settings_data.dart';
import 'package:modern_gauge_flutter/utils/app_constants.dart';

void main() {
  group('SettingsData notificationDurationSeconds', () {
    test('defaults to the configured default', () {
      expect(
        SettingsData().notificationDurationSeconds,
        AppConstants.defaultNotificationDurationSeconds,
      );
    });

    test('survives a JSON round-trip', () {
      final original = SettingsData(notificationDurationSeconds: 17);
      final restored = SettingsData.fromJson(original.toJson());

      expect(restored.notificationDurationSeconds, 17);
    });

    test('fromJson falls back to default when absent', () {
      final restored = SettingsData.fromJson({'wifiEnabled': true});

      expect(
        restored.notificationDurationSeconds,
        AppConstants.defaultNotificationDurationSeconds,
      );
    });

    test('copyWith preserves the value when not overridden', () {
      final base = SettingsData(notificationDurationSeconds: 8);
      final copy = base.copyWith(wifiEnabled: false);

      expect(copy.notificationDurationSeconds, 8);
    });
  });
}
