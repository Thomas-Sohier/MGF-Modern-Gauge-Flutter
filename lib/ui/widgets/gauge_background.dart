import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/ui/themes/gauge_background_theme.dart';

/// A widget that displays a simple circular background with a metallic/textured effect.
/// It uses a radial gradient to simulate the look of a classic gauge face.
class GaugeTexturedBackground extends StatelessWidget {
  /// The widget to display on top of the textured background.
  final Widget? child;

  /// The solid background color.
  final Color? backgroundColor;

  /// The color of the optional outer border.
  final Color? borderColor;

  /// The width of the optional outer border. Set to 0 to disable.
  final double? borderWidth;

  const GaugeTexturedBackground({super.key, this.child, this.backgroundColor, this.borderColor, this.borderWidth});

  @override
  Widget build(BuildContext context) {
    final gaugeThemeBackground = Theme.of(context).extension<GaugeThemeBackground>()!;

    return AspectRatio(
      aspectRatio: 1,
      child: CustomPaint(
        painter: _TexturedBackgroundPainter(
          backgroundColor: backgroundColor ?? gaugeThemeBackground.backgroundColor ?? Color(0xFFE0E0E0),
          borderColor: borderColor ?? gaugeThemeBackground.borderColor ?? Colors.black54,
          borderWidth: borderWidth ?? gaugeThemeBackground.borderWidth ?? 2.0,
        ),
        child: child != null ? Center(child: child) : null,
      ),
    );
  }
}

/// Custom painter for drawing the simplified circular background.
class _TexturedBackgroundPainter extends CustomPainter {
  final Color backgroundColor;
  final Color borderColor;
  final double borderWidth;

  _TexturedBackgroundPainter({required this.backgroundColor, required this.borderColor, required this.borderWidth});

  @override
  void paint(Canvas canvas, Size size) {
    final center = Offset(size.width / 2, size.height / 2);
    final radius = size.width / 2;

    // 1. Dessine le fond avec une couleur unie
    final backgroundPaint = Paint()
      ..color = backgroundColor
      ..style = PaintingStyle.fill;

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
    // Le fond doit se redessiner si la couleur ou la bordure changent.
    return oldDelegate.backgroundColor != backgroundColor ||
        oldDelegate.borderColor != borderColor ||
        oldDelegate.borderWidth != borderWidth;
  }
}
