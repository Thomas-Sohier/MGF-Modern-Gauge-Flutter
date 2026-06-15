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

  // --- Go backend server ---
  // Host/port are overridable at build time:
  //   flutter run --dart-define=SERVER_HOST=192.168.1.10 --dart-define=SERVER_PORT=8080
  static const String serverHost = String.fromEnvironment(
    'SERVER_HOST',
    defaultValue: 'localhost',
  );
  static const int serverPort = int.fromEnvironment(
    'SERVER_PORT',
    defaultValue: 8080,
  );

  static const String serverHttpBase = 'http://$serverHost:$serverPort';
  static const String serverWsBase = 'ws://$serverHost:$serverPort';

  // Derived endpoint URLs (do not hardcode these elsewhere).
  static const String ecuWsUrl = '$serverWsBase/ws';
  static const String nowPlayingWsUrl = '$serverWsBase/ws/nowplaying';
  static const String nowPlayingArtUrl = '$serverHttpBase/api/nowplaying/art';
  static const String navigationWsUrl = '$serverWsBase/ws/navigation';
  static const String navigationIconUrl = '$serverHttpBase/api/navigation/icon';
  static const String notificationsWsUrl = '$serverWsBase/ws/notifications';
}
