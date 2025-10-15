import 'dart:convert';
import 'package:flutter/foundation.dart';
import 'package:modern_gauge_flutter/models/settings_data.dart';
import 'package:shared_preferences/shared_preferences.dart';

/// Service de gestion des paramètres utilisant le pattern Singleton.
///
/// L'instance de SharedPreferences est chargée une seule fois et conservée,
/// optimisant ainsi les accès répétés.
class SettingsService {
  static const String _settingsKey = 'app_settings';

  // --- Implémentation du Singleton ---
  // Instance privée et statique
  static final SettingsService _instance = SettingsService._internal();

  // Constructeur factory qui retourne toujours la même instance
  factory SettingsService() {
    return _instance;
  }

  // Constructeur privé, utilisé uniquement à l'intérieur de la classe
  SettingsService._internal();
  // --- Fin du Singleton ---

  SharedPreferences? _prefs;

  /// Méthode d'initialisation asynchrone.
  /// Doit être appelée une seule fois au démarrage de l'application.
  /// (par exemple, dans votre fonction `main`).
  Future<void> init() async {
    _prefs = await SharedPreferences.getInstance();
    if (kDebugMode) {
      print('SettingsService initialized.');
    }
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
        if (kDebugMode) {
          print('Error decoding settings JSON, returning default settings: $e');
        }
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
  }

  /// Supprime tous les paramètres sauvegardés.
  Future<void> clearSettings() async {
    _ensureInitialized();
    await _prefs!.remove(_settingsKey);
  }
}
