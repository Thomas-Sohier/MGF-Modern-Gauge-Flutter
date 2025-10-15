import 'package:flutter/material.dart';

// Cette classe définit les couleurs personnalisées pour notre horloge analogique.
@immutable
class AnalogClockTheme extends ThemeExtension<AnalogClockTheme> {
  const AnalogClockTheme({
    required this.handColor,
    required this.hourTickColor,
    required this.minuteDotColor,
    required this.numberColor,
    required this.centerPivotColor,
    required this.centerPivotRidgeColor,
    required this.shadowColor,
  });

  final Color? handColor; // Couleur des aiguilles (heure et minute)
  final Color? hourTickColor; // Couleur des marqueurs d'heures principaux
  final Color? minuteDotColor; // Couleur des points pour les minutes
  final Color? numberColor; // Couleur des chiffres (12, 3, 6, 9)
  final Color? centerPivotColor; // Couleur principale du pivot central
  final Color? centerPivotRidgeColor; // Couleur des crêtes sur le pivot
  final Color? shadowColor; // Couleur de l'ombre des aiguilles

  @override
  AnalogClockTheme copyWith({
    Color? handColor,
    Color? hourTickColor,
    Color? minuteDotColor,
    Color? numberColor,
    Color? centerPivotColor,
    Color? centerPivotRidgeColor,
    Color? shadowColor,
  }) {
    return AnalogClockTheme(
      handColor: handColor ?? this.handColor,
      hourTickColor: hourTickColor ?? this.hourTickColor,
      minuteDotColor: minuteDotColor ?? this.minuteDotColor,
      numberColor: numberColor ?? this.numberColor,
      centerPivotColor: centerPivotColor ?? this.centerPivotColor,
      centerPivotRidgeColor: centerPivotRidgeColor ?? this.centerPivotRidgeColor,
      shadowColor: shadowColor ?? this.shadowColor,
    );
  }

  @override
  AnalogClockTheme lerp(ThemeExtension<AnalogClockTheme>? other, double t) {
    if (other is! AnalogClockTheme) {
      return this;
    }
    return AnalogClockTheme(
      handColor: Color.lerp(handColor, other.handColor, t),
      hourTickColor: Color.lerp(hourTickColor, other.hourTickColor, t),
      minuteDotColor: Color.lerp(minuteDotColor, other.minuteDotColor, t),
      numberColor: Color.lerp(numberColor, other.numberColor, t),
      centerPivotColor: Color.lerp(centerPivotColor, other.centerPivotColor, t),
      centerPivotRidgeColor: Color.lerp(centerPivotRidgeColor, other.centerPivotRidgeColor, t),
      shadowColor: Color.lerp(shadowColor, other.shadowColor, t),
    );
  }
}
