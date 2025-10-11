import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/ui/themes/gauge_background_theme.dart';

/// A widget that displays a simple circular background with a metallic/textured effect.
/// It uses a radial gradient to simulate the look of a classic gauge face.
class GaugeTexturedBackground extends StatelessWidget {
  /// The widget to display on top of the textured background.
  final Widget? child;

  /// The color at the center of the gradient.
  final Color? centerColor;

  /// The color at the edge of the gradient.
  final Color? edgeColor;

  /// The color of the optional outer border.
  final Color? borderColor;

  /// The width of the optional outer border. Set to 0 to disable.
  final double? borderWidth;

  const GaugeTexturedBackground({
    super.key,
    this.child,
    this.centerColor,
    this.edgeColor,
    this.borderColor,
    this.borderWidth,
  });

  @override
  Widget build(BuildContext context) {
    final gaugeThemeBackground = Theme.of(context).extension<GaugeThemeBackground>()!;

    return AspectRatio(
      aspectRatio: 1,
      child: CustomPaint(
        painter: _TexturedBackgroundPainter(
          centerColor: centerColor ?? gaugeThemeBackground.centerColor ?? Color(0xFFE0E0E0),
          edgeColor: edgeColor ?? gaugeThemeBackground.edgeColor ?? Color(0xFFBDBDBD),
          borderColor: borderColor ?? gaugeThemeBackground.borderColor ?? Colors.black54,
          borderWidth: borderWidth ?? gaugeThemeBackground.borderWidth ?? 2.0,
        ),
        child: child != null ? Center(child: child) : null,
      ),
    );
  }
}

/// Custom painter for drawing the textured circular background.
class _TexturedBackgroundPainter extends CustomPainter {
  final Color centerColor;
  final Color edgeColor;
  final Color borderColor;
  final double borderWidth;

  _TexturedBackgroundPainter({
    required this.centerColor,
    required this.edgeColor,
    required this.borderColor,
    required this.borderWidth,
  });

  @override
  void paint(Canvas canvas, Size size) {
    final center = Offset(size.width / 2, size.height / 2);
    final radius = size.width / 2;

    // 1. Dessine le fond avec un gradient radial pour l'effet texturé
    final backgroundPaint = Paint()
      ..shader = RadialGradient(
        colors: [centerColor, edgeColor],
      ).createShader(Rect.fromCircle(center: center, radius: radius));

    canvas.drawCircle(center, radius, backgroundPaint);

    // 2. Dessine une bordure extérieure si sa largeur est supérieure à 0
    if (borderWidth > 0) {
      final borderPaint = Paint()
        ..color = borderColor
        ..style = PaintingStyle
            .stroke // Important: dessine seulement le contour
        ..strokeWidth = borderWidth;

      // On dessine le cercle au milieu de l'épaisseur de la bordure pour qu'il soit bien aligné
      canvas.drawCircle(center, radius - borderWidth / 2, borderPaint);
    }
  }

  @override
  bool shouldRepaint(_TexturedBackgroundPainter oldDelegate) {
    // Le fond doit se redessiner si les couleurs ou la bordure changent.
    return oldDelegate.centerColor != centerColor ||
        oldDelegate.edgeColor != edgeColor ||
        oldDelegate.borderColor != borderColor ||
        oldDelegate.borderWidth != borderWidth;
  }
}
