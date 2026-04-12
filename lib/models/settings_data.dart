import 'package:flutter/material.dart';

enum WakeUpMode { onStart, longPress } // Mise en veille si: Démarrage, Appui long

class SettingsData {
  /// Activer/désactiver le son
  final bool soundEnabled;

  /// Chemin de l'image de fond (null si pas d'image)
  final String? backgroundImagePath;

  /// Luminosité de l'écran (0.0 à 1.0)
  final double screenBrightness;

  /// Mode de thème (clair, sombre, système)
  final ThemeMode themeMode;

  /// Délai avant la mise en veille automatique
  final int autoSleepDelaySeconds;

  /// Comment sortir de la veille
  final WakeUpMode wakeUpMode;

  /// Écrans activés (segments de route, ex: '/rpm', '/time')
  final Set<String> enabledScreens;

  static const Set<String> allScreens = {'/rpm', '/time', '/faults', '/music'};

  SettingsData({
    this.soundEnabled = true,
    this.backgroundImagePath,
    this.screenBrightness = 1.0,
    // Par défaut, un thème sombre pour le tableau de bord
    this.themeMode = ThemeMode.dark,
    // 5 minutes par défaut
    this.autoSleepDelaySeconds = 300,
    this.wakeUpMode = WakeUpMode.onStart,
    this.enabledScreens = allScreens,
  });

  SettingsData copyWith({
    bool? soundEnabled,
    String? backgroundImagePath,
    double? screenBrightness,
    ThemeMode? themeMode,
    int? autoSleepDelaySeconds,
    WakeUpMode? wakeUpMode,
    Set<String>? enabledScreens,
  }) {
    return SettingsData(
      soundEnabled: soundEnabled ?? this.soundEnabled,
      backgroundImagePath: backgroundImagePath ?? this.backgroundImagePath,
      screenBrightness: screenBrightness ?? this.screenBrightness,
      themeMode: themeMode ?? this.themeMode,
      autoSleepDelaySeconds: autoSleepDelaySeconds ?? this.autoSleepDelaySeconds,
      wakeUpMode: wakeUpMode ?? this.wakeUpMode,
      enabledScreens: enabledScreens ?? this.enabledScreens,
    );
  }

  /// Factory constructor pour créer une instance de SettingsData à partir d'un map JSON.
  factory SettingsData.fromJson(Map<String, dynamic> json) {
    return SettingsData(
      soundEnabled: json['soundEnabled'] as bool? ?? true,
      backgroundImagePath: json['backgroundImagePath'] as String?,
      screenBrightness: (json['screenBrightness'] as num?)?.toDouble() ?? 1.0,
      themeMode: ThemeMode.values[json['themeMode'] as int? ?? ThemeMode.dark.index],
      autoSleepDelaySeconds: json['autoSleepDelaySeconds'] as int? ?? 300,
      wakeUpMode: WakeUpMode.values[json['wakeUpMode'] as int? ?? WakeUpMode.onStart.index],
      enabledScreens: (json['enabledScreens'] as List<dynamic>?)
              ?.map((e) => e as String)
              .toSet() ??
          allScreens,
    );
  }

  /// Méthode pour convertir une instance de SettingsData en un map JSON.
  Map<String, dynamic> toJson() {
    return {
      'soundEnabled': soundEnabled,
      'backgroundImagePath': backgroundImagePath,
      'screenBrightness': screenBrightness,
      'themeMode': themeMode.index,
      'autoSleepDelaySeconds': autoSleepDelaySeconds,
      'wakeUpMode': wakeUpMode.index,
      'enabledScreens': enabledScreens.toList(),
    };
  }

  @override
  String toString() {
    return 'SettingsData(soundEnabled: $soundEnabled, backgroundImagePath: $backgroundImagePath, screenBrightness: $screenBrightness, themeMode: $themeMode, autoSleepDelaySeconds: $autoSleepDelaySeconds, wakeUpMode: $wakeUpMode, enabledScreens: $enabledScreens)';
  }
}
