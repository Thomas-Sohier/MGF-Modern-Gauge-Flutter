import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/ui/themes/gauge_background_theme.dart';
import 'package:modern_gauge_flutter/ui/themes/gauge_theme.dart';

/// Defines the color and typography themes for the entire application.
class AppTheme {
  // --- Common Colors ---
  static const Color primaryColor = Colors.lightBlueAccent; // Accent color for gauges/interactions
  static const Color accentColor = Color(0xFF673AB7); // Another accent if needed
  static const Color textColorLight = Colors.black87;
  static const Color textColorDark = Colors.white;

  // --- Dark Theme ---
  static final ThemeData darkTheme = ThemeData(
    brightness: Brightness.dark,
    primaryColor: primaryColor,
    colorScheme: const ColorScheme.dark(
      primary: primaryColor,
      secondary: accentColor,
      surface: Color(0xFF1E1E1E), // Main background color
      onPrimary: Colors.white,
      onSecondary: Colors.white,
      onSurface: Colors.white70,
      error: Colors.redAccent,
      onError: Colors.white,
    ),
    scaffoldBackgroundColor: Colors.black, // Global background for Scaffolds
    appBarTheme: const AppBarTheme(
      backgroundColor: Colors.transparent, // AppBar background
      foregroundColor: textColorDark, // AppBar icon and text color
      elevation: 0,
      centerTitle: true,
    ),
    textTheme: const TextTheme(
      // Define your text styles here for consistency
      displayLarge: TextStyle(color: textColorDark, fontSize: 57),
      displayMedium: TextStyle(color: textColorDark, fontSize: 45),
      displaySmall: TextStyle(color: textColorDark, fontSize: 36),
      headlineLarge: TextStyle(color: textColorDark, fontSize: 32),
      headlineMedium: TextStyle(color: textColorDark, fontSize: 28),
      headlineSmall: TextStyle(color: textColorDark, fontSize: 24),
      titleLarge: TextStyle(color: textColorDark, fontSize: 22),
      titleMedium: TextStyle(color: textColorDark, fontSize: 16),
      titleSmall: TextStyle(color: textColorDark, fontSize: 14),
      bodyLarge: TextStyle(color: textColorDark, fontSize: 16),
      bodyMedium: TextStyle(color: textColorDark, fontSize: 14),
      bodySmall: TextStyle(color: Colors.white70, fontSize: 12),
      labelLarge: TextStyle(color: textColorDark, fontSize: 14),
      labelMedium: TextStyle(color: Colors.white70, fontSize: 12),
      labelSmall: TextStyle(color: Colors.white70, fontSize: 11),
    ),
    iconTheme: const IconThemeData(
      color: textColorDark, // Default icon color
    ),
    extensions: const <ThemeExtension<dynamic>>[
      GaugeTheme(
        activeColor: Colors.white,
        inactiveColor: Color(0xFF303030),
        dangerColor: Colors.redAccent,
        dangerInactiveColor: Color(0x33FF0000),
        borderColor: Colors.white54,
      ),
      GaugeThemeBackground(
        centerColor: Color.fromARGB(255, 39, 39, 39),
        edgeColor: Color.fromARGB(255, 14, 14, 14),
        borderColor: Colors.black,
        borderWidth: 2.0,
      ),
    ],
  );

  // --- Light Theme (Optional, but good to have a base) ---
  static final ThemeData lightTheme = ThemeData(
    brightness: Brightness.light,
    primaryColor: primaryColor,
    colorScheme: const ColorScheme.light(
      primary: primaryColor,
      secondary: accentColor,
      surface: Colors.white,
      onPrimary: Colors.white,
      onSecondary: Colors.white,
      onSurface: Colors.black87,
      error: Colors.red,
      onError: Colors.white,
    ),
    scaffoldBackgroundColor: Colors.white,
    appBarTheme: const AppBarTheme(
      backgroundColor: primaryColor,
      foregroundColor: textColorLight,
      elevation: 2,
      centerTitle: true,
    ),
    textTheme: const TextTheme(
      displayLarge: TextStyle(color: textColorLight, fontSize: 57),
      displayMedium: TextStyle(color: textColorLight, fontSize: 45),
      displaySmall: TextStyle(color: textColorLight, fontSize: 36),
      headlineLarge: TextStyle(color: textColorLight, fontSize: 32),
      headlineMedium: TextStyle(color: textColorLight, fontSize: 28),
      headlineSmall: TextStyle(color: textColorLight, fontSize: 24),
      titleLarge: TextStyle(color: textColorLight, fontSize: 22),
      titleMedium: TextStyle(color: textColorLight, fontSize: 16),
      titleSmall: TextStyle(color: textColorLight, fontSize: 14),
      bodyLarge: TextStyle(color: textColorLight, fontSize: 16),
      bodyMedium: TextStyle(color: textColorLight, fontSize: 14),
      bodySmall: TextStyle(color: Colors.black54, fontSize: 12),
      labelLarge: TextStyle(color: textColorLight, fontSize: 14),
      labelMedium: TextStyle(color: textColorLight, fontSize: 12),
      labelSmall: TextStyle(color: Colors.black54, fontSize: 11),
    ),
    iconTheme: const IconThemeData(color: textColorLight),
    extensions: <ThemeExtension<dynamic>>[
      GaugeTheme(
        activeColor: Colors.black,
        inactiveColor: Colors.grey.shade400,
        dangerColor: Colors.red.shade700,
        dangerInactiveColor: Colors.red.withValues(alpha: 0.15),
        borderColor: Colors.black54,
      ),
      GaugeThemeBackground(
        centerColor: Color(0xFFE0E0E0),
        edgeColor: Color(0xFFBDBDBD),
        borderColor: Colors.black54,
        borderWidth: 2.0,
      ),
    ],
  );
}
