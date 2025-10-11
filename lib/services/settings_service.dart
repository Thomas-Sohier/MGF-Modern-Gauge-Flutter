import 'dart:convert'; // Pour encoder/décoder en JSON
import 'package:shared_preferences/shared_preferences.dart';

import 'package:modern_gauge_flutter/models/settings_data.dart';

class SettingsService {
  static const String _settingsKey = 'app_settings'; // Clé pour stocker les paramètres

  // Méthode pour charger les paramètres depuis le stockage local
  static Future<SettingsData> loadSettings() async {
    final prefs = await SharedPreferences.getInstance();
    final settingsJson = prefs.getString(_settingsKey);

    if (settingsJson != null) {
      try {
        final Map<String, dynamic> decodedJson = json.decode(settingsJson);
        return _settingsFromJson(decodedJson);
      } catch (e) {
        print('Error decoding settings JSON: $e');
        return SettingsData();
      }
    }
    return SettingsData();
  }

  // Méthode pour sauvegarder les paramètres dans le stockage local
  static Future<void> saveSettings(SettingsData settings) async {
    final prefs = await SharedPreferences.getInstance();
    final String encodedJson = json.encode(_settingsToJson(settings));
    await prefs.setString(_settingsKey, encodedJson);
  }

  static Map<String, dynamic> _settingsToJson(SettingsData settings) {
    return {
      'soundEnabled': settings.soundEnabled,
      'backgroundImagePath': settings.backgroundImagePath,
      'screenBrightness': settings.screenBrightness,
      'themeMode': settings.themeMode.index,
      'autoSleepDelaySeconds': settings.autoSleepDelaySeconds,
      'wakeUpMode': settings.wakeUpMode.index,
    };
  }

  // Convertit un Map<String, dynamic> en objet SettingsData pour le chargement
  static SettingsData _settingsFromJson(Map<String, dynamic> json) {
    return SettingsData(
      soundEnabled: json['soundEnabled'] as bool? ?? true,
      backgroundImagePath: json['backgroundImagePath'] as String?,
      screenBrightness: (json['screenBrightness'] as num?)?.toDouble() ?? 1.0,
      themeMode: ThemeModeOption.values[json['themeMode'] as int? ?? ThemeModeOption.dark.index],
      autoSleepDelaySeconds: json['autoSleepDelaySeconds'] as int? ?? 300,
      wakeUpMode: WakeUpMode.values[json['wakeUpMode'] as int? ?? WakeUpMode.onStart.index],
    );
  }

  // Optionnel: Méthode pour supprimer tous les paramètres sauvegardés
  static Future<void> clearSettings() async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.remove(_settingsKey);
  }
}
