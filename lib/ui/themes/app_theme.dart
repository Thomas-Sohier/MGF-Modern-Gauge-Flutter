import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/ui/themes/clock_theme.dart';
import 'package:modern_gauge_flutter/ui/themes/gauge_background_theme.dart';
import 'package:modern_gauge_flutter/ui/themes/gauge_theme.dart';

/// Palette centrale — toutes les couleurs en un seul endroit.
class AppColors {
  // ── Vert signature ──────────────────────────────────────────────────────
  /// Vert vif : textes/icônes actifs sur fond sombre.
  static const Color greenBright = Color(0xFF00C47A);

  /// Vert profond : textes/icônes actifs sur fond clair.
  static const Color greenDeep = Color(0xFF006B3C);

  // ── Dark palette ────────────────────────────────────────────────────────
  static const Color darkBg = Color(0xFF1C1C1E); // fond global
  static const Color darkSurface = Color(0xFF2C2C2E); // cartes / surfaces
  static const Color darkSurfaceVariant = Color(0xFF3A3A3C); // séparateurs
  static const Color darkOnSurface = Color(0xFFF5F5F5); // texte principal
  static const Color darkOnSurfaceDim = Color(0xFF8E8E93); // texte secondaire
  static const Color darkBorder = Color(0xFF48484A); // bordures
  static const Color darkGaugeBg = Color(0xFF242426); // fond du cercle jauge
  static const Color darkGaugeInactive = Color(0xFF3A3A3C);

  // ── Light palette ───────────────────────────────────────────────────────
  static const Color lightBg = Color(0xFFF2F2F7); // fond global
  static const Color lightSurface = Color(0xFFFFFFFF);
  static const Color lightOnSurface = Color(0xFF1C1C1E); // texte principal
  static const Color lightOnSurfaceDim = Color(0xFF6E6E73); // texte secondaire
  static const Color lightBorder = Color(0xFFC7C7CC);
  static const Color lightGaugeBg = Color(0xFFE5E5EA); // fond du cercle jauge
  static const Color lightGaugeInactive = Color(0xFFD1D1D6);

  // ── Commun ──────────────────────────────────────────────────────────────
  static const Color danger = Color(0xFFFF453A);
  static const Color dangerDim = Color(0x33FF453A);
  static const Color clockHand = Color(0xFFB82B2B); // rouge original, identique dark/light
}

class AppTheme {
  // ── Dark Theme ─────────────────────────────────────────────────────────
  static final ThemeData darkTheme = ThemeData(
    brightness: Brightness.dark,
    primaryColor: AppColors.greenBright,
    colorScheme: const ColorScheme.dark(
      primary: AppColors.greenBright,
      secondary: AppColors.greenBright,
      surface: AppColors.darkSurface,
      onPrimary: AppColors.darkBg,
      onSecondary: AppColors.darkBg,
      onSurface: AppColors.darkOnSurface,
      outline: AppColors.darkBorder,
      error: AppColors.danger,
      onError: AppColors.darkOnSurface,
    ),
    scaffoldBackgroundColor: AppColors.darkBg,
    appBarTheme: const AppBarTheme(
      backgroundColor: Colors.transparent,
      foregroundColor: AppColors.darkOnSurface,
      elevation: 0,
      centerTitle: true,
    ),
    textTheme: const TextTheme(
      displayLarge: TextStyle(color: AppColors.darkOnSurface, fontSize: 57),
      displayMedium: TextStyle(color: AppColors.darkOnSurface, fontSize: 45),
      displaySmall: TextStyle(color: AppColors.darkOnSurface, fontSize: 36),
      headlineLarge: TextStyle(color: AppColors.darkOnSurface, fontSize: 32),
      headlineMedium: TextStyle(color: AppColors.darkOnSurface, fontSize: 28),
      headlineSmall: TextStyle(color: AppColors.darkOnSurface, fontSize: 24),
      titleLarge: TextStyle(color: AppColors.darkOnSurface, fontSize: 22),
      titleMedium: TextStyle(color: AppColors.darkOnSurface, fontSize: 16),
      titleSmall: TextStyle(color: AppColors.darkOnSurface, fontSize: 14),
      bodyLarge: TextStyle(color: AppColors.darkOnSurface, fontSize: 16),
      bodyMedium: TextStyle(color: AppColors.darkOnSurface, fontSize: 14),
      bodySmall: TextStyle(color: AppColors.darkOnSurfaceDim, fontSize: 12),
      labelLarge: TextStyle(color: AppColors.darkOnSurface, fontSize: 14),
      labelMedium: TextStyle(color: AppColors.darkOnSurfaceDim, fontSize: 12),
      labelSmall: TextStyle(color: AppColors.darkOnSurfaceDim, fontSize: 11),
    ),
    iconTheme: const IconThemeData(color: AppColors.darkOnSurface),
    extensions: <ThemeExtension<dynamic>>[
      GaugeTheme(
        activeColor: Colors.white,
        inactiveColor: AppColors.darkGaugeInactive,
        dangerColor: AppColors.danger,
        dangerInactiveColor: AppColors.dangerDim,
        borderColor: AppColors.darkBorder,
      ),
      AnalogClockTheme(
        handColor: AppColors.clockHand,
        hourTickColor: AppColors.darkOnSurface,
        minuteDotColor: AppColors.darkOnSurfaceDim,
        numberColor: AppColors.darkOnSurface,
        centerPivotColor: AppColors.darkOnSurface,
        centerPivotRidgeColor: AppColors.darkOnSurfaceDim,
        shadowColor: Colors.black54,
      ),
      GaugeThemeBackground(
        backgroundColor: AppColors.darkGaugeBg,
        borderColor: AppColors.darkBg,
        borderWidth: 2.0,
      ),
    ],
  );

  // ── Light Theme ────────────────────────────────────────────────────────
  static final ThemeData lightTheme = ThemeData(
    brightness: Brightness.light,
    primaryColor: AppColors.greenDeep,
    colorScheme: const ColorScheme.light(
      primary: AppColors.greenDeep,
      secondary: AppColors.greenDeep,
      surface: AppColors.lightSurface,
      onPrimary: Colors.white,
      onSecondary: Colors.white,
      onSurface: AppColors.lightOnSurface,
      outline: AppColors.lightBorder,
      error: Color(0xFFD70015),
      onError: Colors.white,
    ),
    scaffoldBackgroundColor: AppColors.lightBg,
    appBarTheme: const AppBarTheme(
      backgroundColor: Colors.transparent,
      foregroundColor: AppColors.lightOnSurface,
      elevation: 0,
      centerTitle: true,
    ),
    textTheme: const TextTheme(
      displayLarge: TextStyle(color: AppColors.lightOnSurface, fontSize: 57),
      displayMedium: TextStyle(color: AppColors.lightOnSurface, fontSize: 45),
      displaySmall: TextStyle(color: AppColors.lightOnSurface, fontSize: 36),
      headlineLarge: TextStyle(color: AppColors.lightOnSurface, fontSize: 32),
      headlineMedium: TextStyle(color: AppColors.lightOnSurface, fontSize: 28),
      headlineSmall: TextStyle(color: AppColors.lightOnSurface, fontSize: 24),
      titleLarge: TextStyle(color: AppColors.lightOnSurface, fontSize: 22),
      titleMedium: TextStyle(color: AppColors.lightOnSurface, fontSize: 16),
      titleSmall: TextStyle(color: AppColors.lightOnSurface, fontSize: 14),
      bodyLarge: TextStyle(color: AppColors.lightOnSurface, fontSize: 16),
      bodyMedium: TextStyle(color: AppColors.lightOnSurface, fontSize: 14),
      bodySmall: TextStyle(color: AppColors.lightOnSurfaceDim, fontSize: 12),
      labelLarge: TextStyle(color: AppColors.lightOnSurface, fontSize: 14),
      labelMedium: TextStyle(color: AppColors.lightOnSurfaceDim, fontSize: 12),
      labelSmall: TextStyle(color: AppColors.lightOnSurfaceDim, fontSize: 11),
    ),
    iconTheme: const IconThemeData(color: AppColors.lightOnSurface),
    extensions: <ThemeExtension<dynamic>>[
      GaugeTheme(
        activeColor: Colors.black,
        inactiveColor: AppColors.lightGaugeInactive,
        dangerColor: Color(0xFFD70015),
        dangerInactiveColor: Color(0x22D70015),
        borderColor: AppColors.lightBorder,
      ),
      AnalogClockTheme(
        handColor: AppColors.clockHand,
        hourTickColor: AppColors.lightOnSurface,
        minuteDotColor: AppColors.lightOnSurfaceDim,
        numberColor: AppColors.lightOnSurface,
        centerPivotColor: AppColors.lightOnSurface,
        centerPivotRidgeColor: AppColors.lightOnSurfaceDim,
        shadowColor: Colors.black26,
      ),
      GaugeThemeBackground(
        backgroundColor: AppColors.lightGaugeBg,
        borderColor: AppColors.lightBorder,
        borderWidth: 2.0,
      ),
    ],
  );
}
