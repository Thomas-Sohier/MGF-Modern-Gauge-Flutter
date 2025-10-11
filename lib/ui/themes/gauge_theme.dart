import 'package:flutter/material.dart';

// Cette classe définit les couleurs personnalisées pour notre jauge.
@immutable
class GaugeTheme extends ThemeExtension<GaugeTheme> {
  const GaugeTheme({
    required this.activeColor,
    required this.inactiveColor,
    required this.dangerColor,
    required this.dangerInactiveColor,
    required this.borderColor,
  });

  final Color? activeColor;
  final Color? inactiveColor;
  final Color? dangerColor;
  final Color? dangerInactiveColor;
  final Color? borderColor;

  @override
  GaugeTheme copyWith({
    Color? activeColor,
    Color? inactiveColor,
    Color? dangerColor,
    Color? dangerInactiveColor,
    Color? borderColor,
  }) {
    return GaugeTheme(
      activeColor: activeColor ?? this.activeColor,
      inactiveColor: inactiveColor ?? this.inactiveColor,
      dangerColor: dangerColor ?? this.dangerColor,
      dangerInactiveColor: dangerInactiveColor ?? this.dangerInactiveColor,
      borderColor: borderColor ?? this.borderColor,
    );
  }

  @override
  GaugeTheme lerp(ThemeExtension<GaugeTheme>? other, double t) {
    if (other is! GaugeTheme) {
      return this;
    }
    return GaugeTheme(
      activeColor: Color.lerp(activeColor, other.activeColor, t),
      inactiveColor: Color.lerp(inactiveColor, other.inactiveColor, t),
      dangerColor: Color.lerp(dangerColor, other.dangerColor, t),
      dangerInactiveColor: Color.lerp(dangerInactiveColor, other.dangerInactiveColor, t),
      borderColor: Color.lerp(borderColor, other.borderColor, t),
    );
  }
}
