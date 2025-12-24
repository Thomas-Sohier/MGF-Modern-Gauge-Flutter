import 'dart:ui';

import 'package:flutter/material.dart';

// Cette classe définit les couleurs personnalisées pour notre jauge.
@immutable
class GaugeThemeBackground extends ThemeExtension<GaugeThemeBackground> {
  const GaugeThemeBackground({required this.backgroundColor, required this.borderColor, required this.borderWidth});

  final Color? backgroundColor;
  final Color? borderColor;
  final double? borderWidth;

  @override
  GaugeThemeBackground copyWith({Color? backgroundColor, Color? borderColor, double? borderWidth}) {
    return GaugeThemeBackground(
      backgroundColor: backgroundColor ?? this.backgroundColor,
      borderColor: borderColor ?? this.borderColor,
      borderWidth: borderWidth ?? this.borderWidth,
    );
  }

  @override
  GaugeThemeBackground lerp(ThemeExtension<GaugeThemeBackground>? other, double t) {
    if (other is! GaugeThemeBackground) {
      return this;
    }
    return GaugeThemeBackground(
      backgroundColor: Color.lerp(backgroundColor, other.backgroundColor, t),
      borderColor: Color.lerp(borderColor, other.borderColor, t),
      borderWidth: lerpDouble(borderWidth, other.borderWidth, t),
    );
  }
}
