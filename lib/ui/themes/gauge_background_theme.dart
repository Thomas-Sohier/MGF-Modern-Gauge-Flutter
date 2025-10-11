import 'dart:ui';

import 'package:flutter/material.dart';

// Cette classe définit les couleurs personnalisées pour notre jauge.
@immutable
class GaugeThemeBackground extends ThemeExtension<GaugeThemeBackground> {
  const GaugeThemeBackground({
    required this.centerColor,
    required this.edgeColor,
    required this.borderColor,
    required this.borderWidth,
  });

  final Color? centerColor;
  final Color? edgeColor;
  final Color? borderColor;
  final double? borderWidth;

  @override
  GaugeThemeBackground copyWith({Color? centerColor, Color? edgeColor, Color? borderColor, double? borderWidth}) {
    return GaugeThemeBackground(
      centerColor: centerColor ?? this.centerColor,
      edgeColor: edgeColor ?? this.edgeColor,
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
      centerColor: Color.lerp(centerColor, other.centerColor, t),
      edgeColor: Color.lerp(edgeColor, other.edgeColor, t),
      borderColor: Color.lerp(borderColor, other.borderColor, t),
      borderWidth: lerpDouble(borderWidth, other.borderWidth, t),
    );
  }
}
