import 'dart:convert';
import 'package:modern_gauge_flutter/models/settings_data.dart';
import 'package:modern_gauge_flutter/services/log_service.dart';
import 'package:shared_preferences/shared_preferences.dart';

/// Service de gestion des paramètres
class SettingsService {
  static const String _settingsKey = 'app_settings';
  static final SettingsService _instance = SettingsService._internal();
  SettingsService._internal();
  SharedPreferences? _prefs;

  factory SettingsService() {
    return _instance;
  }

  Future<void> init() async {
    _prefs = await SharedPreferences.getInstance();
    LogService.info('[SettingsService] - initialized.');
  }

  // S'assure que le service a été initialisé avant utilisation.
  void _ensureInitialized() {
    if (_prefs == null) {
      throw Exception('SettingsService not initialized. Call init() before using.');
    }
  }

  /// Charge les paramètres depuis le stockage local.
  /// Retourne un objet SettingsData avec les valeurs par défaut si rien n'est trouvé.
  SettingsData loadSettings() {
    _ensureInitialized();
    final settingsJson = _prefs!.getString(_settingsKey);

    if (settingsJson != null) {
      try {
        final Map<String, dynamic> decodedJson = json.decode(settingsJson);
        return SettingsData.fromJson(decodedJson);
      } catch (e) {
        LogService.error('[SettingsService] - Error decoding settings JSON, returning default settings: $e');
        return SettingsData();
      }
    }
    return SettingsData();
  }

  /// Sauvegarde un objet SettingsData dans le stockage local.
  Future<void> saveSettings(SettingsData settings) async {
    _ensureInitialized();
    final String encodedJson = json.encode(settings.toJson());
    await _prefs!.setString(_settingsKey, encodedJson);
    LogService.info('[SettingsService] - Settings saved $encodedJson.');
  }

  /// Supprime tous les paramètres sauvegardés.
  Future<void> clearSettings() async {
    _ensureInitialized();
    await _prefs!.remove(_settingsKey);
  }
}
