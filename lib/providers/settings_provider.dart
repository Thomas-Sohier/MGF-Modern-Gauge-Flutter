import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/models/settings_data.dart';
// import 'package:modern_gauge_flutter/services/settings_service.dart';

class SettingsProvider with ChangeNotifier {
  SettingsData _settings = SettingsData();

  SettingsData get settings => _settings;

  Future<void> loadSettings() async {
    // _settings = await SettingsService.loadSettings();
    notifyListeners();
  }

  void updateSettings(SettingsData newSettings) {
    _settings = newSettings;
    notifyListeners();
    // SettingsService.saveSettings(_settings);
  }

  void toggleSound(bool enabled) {
    _settings = _settings.copyWith(soundEnabled: enabled);
    notifyListeners();
    // SettingsService.saveSettings(_settings);
  }

  void setBackgroundImage(String? path) {
    _settings = _settings.copyWith(backgroundImagePath: path);
    notifyListeners();
    // SettingsService.saveSettings(_settings);
  }

  void setScreenBrightness(double brightness) {
    _settings = _settings.copyWith(screenBrightness: brightness);
    notifyListeners();
    // SettingsService.saveSettings(_settings);
  }

  void setThemeMode(ThemeMode mode) {
    _settings = _settings.copyWith(themeMode: mode);
    notifyListeners();
    // SettingsService.saveSettings(_settings);
  }

  void setAutoSleepDelay(int delaySeconds) {
    _settings = _settings.copyWith(autoSleepDelaySeconds: delaySeconds);
    notifyListeners();
    // SettingsService.saveSettings(_settings);
  }

  void setWakeUpMode(WakeUpMode mode) {
    _settings = _settings.copyWith(wakeUpMode: mode);
    notifyListeners();
    // SettingsService.saveSettings(_settings);
  }

  void resetSettings() {
    _settings = SettingsData();
    notifyListeners();
    // SettingsService.saveSettings(_settings);
  }
}
