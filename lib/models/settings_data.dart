enum ThemeModeOption { system, light, dark }

enum WakeUpMode { onStart, longPress } // Mise en veille si: Démarrage, Appui long

class SettingsData {
  /// Activer/désactiver le son
  final bool soundEnabled;

  /// Chemin de l'image de fond (null si pas d'image)
  final String? backgroundImagePath;

  /// Luminosité de l'écran (0.0 à 1.0)
  final double screenBrightness;

  /// Mode de thème (clair, sombre, système)
  final ThemeModeOption themeMode;

  /// Délai avant la mise en veille automatique
  final int autoSleepDelaySeconds;

  /// Comment sortir de la veille
  final WakeUpMode wakeUpMode;

  SettingsData({
    this.soundEnabled = true,
    this.backgroundImagePath,
    this.screenBrightness = 1.0,
    // Par défaut, un thème sombre pour le tableau de bord
    this.themeMode = ThemeModeOption.dark,
    // 5 minutes par défaut
    this.autoSleepDelaySeconds = 300,
    this.wakeUpMode = WakeUpMode.onStart,
  });

  SettingsData copyWith({
    bool? soundEnabled,
    String? backgroundImagePath,
    double? screenBrightness,
    ThemeModeOption? themeMode,
    int? autoSleepDelaySeconds,
    WakeUpMode? wakeUpMode,
  }) {
    return SettingsData(
      soundEnabled: soundEnabled ?? this.soundEnabled,
      backgroundImagePath: backgroundImagePath ?? this.backgroundImagePath,
      screenBrightness: screenBrightness ?? this.screenBrightness,
      themeMode: themeMode ?? this.themeMode,
      autoSleepDelaySeconds: autoSleepDelaySeconds ?? this.autoSleepDelaySeconds,
      wakeUpMode: wakeUpMode ?? this.wakeUpMode,
    );
  }

  @override
  String toString() {
    return 'SettingsData(soundEnabled: $soundEnabled, backgroundImagePath: $backgroundImagePath, screenBrightness: $screenBrightness, themeMode: $themeMode, autoSleepDelaySeconds: $autoSleepDelaySeconds, wakeUpMode: $wakeUpMode)';
  }
}
