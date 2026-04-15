import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/models/settings_data.dart';
import 'package:modern_gauge_flutter/services/settings_service.dart';

class SettingsProvider with ChangeNotifier {
  SettingsData _settings = SettingsData();

  SettingsData get settings => _settings;

  SettingsProvider() : _settings = SettingsService().loadSettings();

  void _update(SettingsData s) {
    _settings = s;
    notifyListeners();
    SettingsService().saveSettings(s);
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
