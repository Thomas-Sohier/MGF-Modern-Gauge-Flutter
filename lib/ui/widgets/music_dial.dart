import 'dart:math';
import 'package:flutter/material.dart';

/// Jauge circulaire (complète ou partielle) affichant une progression.
class MusicDial extends StatelessWidget {
  /// La progression actuelle à afficher (de 0.0 à 1.0).
  final double progress;

  /// La couleur de fond de la jauge (la partie inactive).
  final Color backgroundColor;

  /// La couleur de premier plan de la jauge (la partie qui représente le progrès).
  final Color foregroundColor;

  /// L'épaisseur du trait de la jauge.
  final double strokeWidth;

  /// Le facteur déterminant la portion du cercle à dessiner (de 0.0 à 1.0).
  /// 1.0 pour un cercle complet (360°), 0.75 pour un arc de 270°, etc.
  final double sweepFactor;

  const MusicDial({
    super.key,
    required this.progress,
    required this.backgroundColor,
    required this.foregroundColor,
    this.strokeWidth = 14.0,
    this.sweepFactor = 1.0,
  }) : assert(sweepFactor >= 0.0 && sweepFactor <= 1.0);

  @override
  Widget build(BuildContext context) {
    return CustomPaint(
      size: Size.infinite,
      painter: _MusicDialPainter(
        progress: progress,
        backgroundColor: backgroundColor,
        foregroundColor: foregroundColor,
        strokeWidth: strokeWidth,
        sweepFactor: sweepFactor,
      ),
    );
  }
}

class _MusicDialPainter extends CustomPainter {
  final double progress;
  final Color backgroundColor;
  final Color foregroundColor;
  final double strokeWidth;
  final double sweepFactor;

  _MusicDialPainter({
    required this.progress,
    required this.backgroundColor,
    required this.foregroundColor,
    required this.strokeWidth,
    required this.sweepFactor,
  });

  @override
  void paint(Canvas canvas, Size size) {
    final center = Offset(size.width / 2, size.height / 2);
    final radius = size.width / 2;

    final totalSweepAngle = 2 * pi * sweepFactor;
    final gapAngle = 2 * pi * (1 - sweepFactor);
    // Centre le "trou" en bas. L'angle 0 est à droite (3h), pi/2 est en bas (6h).
    final startAngle = (pi / 2) + (gapAngle / 2);

    final backgroundPaint = Paint()
      ..color = backgroundColor
      ..style = PaintingStyle.stroke
      ..strokeWidth = strokeWidth
      ..strokeCap = StrokeCap.round;

    final foregroundPaint = Paint()
      ..color = foregroundColor
      ..style = PaintingStyle.stroke
      ..strokeWidth = strokeWidth
      ..strokeCap = StrokeCap.round;

    final rect = Rect.fromCircle(
      center: center,
      radius: radius - strokeWidth / 2,
    );
    canvas.drawArc(rect, startAngle, totalSweepAngle, false, backgroundPaint);
    canvas.drawArc(
      rect,
      startAngle,
      totalSweepAngle * progress,
      false,
      foregroundPaint,
    );
  }

  @override
  bool shouldRepaint(covariant _MusicDialPainter oldDelegate) {
    return progress != oldDelegate.progress ||
        backgroundColor != oldDelegate.backgroundColor ||
        foregroundColor != oldDelegate.foregroundColor ||
        strokeWidth != oldDelegate.strokeWidth ||
        sweepFactor != oldDelegate.sweepFactor;
  }
}
