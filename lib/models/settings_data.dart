import 'package:flutter/material.dart';

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

  static const Set<String> allScreens = {'/rpm', '/time', '/faults', '/music'};

  SettingsData({
    this.backgroundImagePath,
    this.themeMode = ThemeMode.dark,
    this.enabledScreens = allScreens,
  });

  SettingsData copyWith({
    String? backgroundImagePath,
    ThemeMode? themeMode,
    Set<String>? enabledScreens,
  }) {
    return SettingsData(
      backgroundImagePath: backgroundImagePath ?? this.backgroundImagePath,
      themeMode: themeMode ?? this.themeMode,
      enabledScreens: enabledScreens ?? this.enabledScreens,
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
    );
  }

  /// Méthode pour convertir une instance de SettingsData en un map JSON.
  Map<String, dynamic> toJson() {
    return {
      'backgroundImagePath': backgroundImagePath,
      'themeMode': themeMode.index,
      'enabledScreens': enabledScreens.toList(),
    };
  }

  @override
  String toString() {
    return 'SettingsData(backgroundImagePath: $backgroundImagePath, themeMode: $themeMode, enabledScreens: $enabledScreens)';
  }
}
