/// A utility class to hold various constant values used across the application.
class AppConstants {
  // --- General App Info ---
  static const String appName = 'ODB Dashboard';
  static const String appVersion = '1.0.0';
  static const String mgLogoAssetPath = 'assets/images/mg_logo.png';

  // --- UI/Theming Constants ---
  static const double defaultPadding = 16.0;
  static const double defaultRadius = 8.0;

  // --- Settings Defaults / Limits (if not managed by SettingsData) ---
  // You might want to define min/max values for sliders or delays here
  static const int minAutoSleepDelaySeconds = 0; // Never sleep
  static const int maxAutoSleepDelaySeconds = 3600; // 1 hour
  static const double minScreenBrightness = 0.1;
  static const double maxScreenBrightness = 1.0;

  // --- Animation Durations ---
  static const Duration defaultAnimationDuration = Duration(milliseconds: 300);
  static const Duration splashScreenDelay = Duration(seconds: 2);
}
