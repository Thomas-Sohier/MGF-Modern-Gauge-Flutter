import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/utils/app_constants.dart';

enum WakeUpMode {
  onStart,
  longPress,
} // Mise en veille si: Démarrage, Appui long

class SettingsData {
  /// Chemin de l'image de fond (null si pas d'image)
  final String? backgroundImagePath;

  /// Mode de thème (clair, sombre, système)
  final ThemeMode themeMode;

  /// Écrans activés (segments de route, ex: '/rpm', '/time')
  final Set<String> enabledScreens;

  /// WiFi activé (piloté côté appareil via le serveur Go / rfkill)
  final bool wifiEnabled;

  /// Durée d'affichage de l'écran de notification, en secondes.
  final int notificationDurationSeconds;

  static const Set<String> allScreens = {
    '/rpm',
    '/time',
    '/faults',
    '/music',
    '/navigation',
    '/temps',
    '/injection',
    '/lambda',
    '/allumage',
    '/ralenti',
    '/admission',
  };

  SettingsData({
    this.backgroundImagePath,
    this.themeMode = ThemeMode.dark,
    this.enabledScreens = allScreens,
    this.wifiEnabled = true,
    this.notificationDurationSeconds =
        AppConstants.defaultNotificationDurationSeconds,
  });

  SettingsData copyWith({
    String? backgroundImagePath,
    ThemeMode? themeMode,
    Set<String>? enabledScreens,
    bool? wifiEnabled,
    int? notificationDurationSeconds,
  }) {
    return SettingsData(
      backgroundImagePath: backgroundImagePath ?? this.backgroundImagePath,
      themeMode: themeMode ?? this.themeMode,
      enabledScreens: enabledScreens ?? this.enabledScreens,
      wifiEnabled: wifiEnabled ?? this.wifiEnabled,
      notificationDurationSeconds:
          notificationDurationSeconds ?? this.notificationDurationSeconds,
    );
  }

  /// Factory constructor pour créer une instance de SettingsData à partir d'un map JSON.
  factory SettingsData.fromJson(Map<String, dynamic> json) {
    return SettingsData(
      backgroundImagePath: json['backgroundImagePath'] as String?,
      themeMode:
          ThemeMode.values[json['themeMode'] as int? ?? ThemeMode.dark.index],
      enabledScreens:
          (json['enabledScreens'] as List<dynamic>?)
              ?.map((e) => e as String)
              .toSet() ??
          allScreens,
      wifiEnabled: json['wifiEnabled'] as bool? ?? true,
      notificationDurationSeconds:
          json['notificationDurationSeconds'] as int? ??
          AppConstants.defaultNotificationDurationSeconds,
    );
  }

  /// Méthode pour convertir une instance de SettingsData en un map JSON.
  Map<String, dynamic> toJson() {
    return {
      'backgroundImagePath': backgroundImagePath,
      'themeMode': themeMode.index,
      'enabledScreens': enabledScreens.toList(),
      'wifiEnabled': wifiEnabled,
      'notificationDurationSeconds': notificationDurationSeconds,
    };
  }

  @override
  String toString() {
    return 'SettingsData(backgroundImagePath: $backgroundImagePath, themeMode: $themeMode, enabledScreens: $enabledScreens, wifiEnabled: $wifiEnabled, notificationDurationSeconds: $notificationDurationSeconds)';
  }
}
