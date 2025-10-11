import 'dart:math';
import 'package:flutter/material.dart';

/// Un CustomPainter qui dessine une jauge circulaire (complète ou partielle).
///
/// Il est hautement configurable pour le progrès, les couleurs, l'épaisseur
/// et l'angle total de la jauge.
class MusicDial extends CustomPainter {
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

  MusicDial({
    required this.progress,
    required this.backgroundColor,
    required this.foregroundColor,
    this.strokeWidth = 14.0,
    this.sweepFactor = 1.0,
  }) : assert(sweepFactor >= 0.0 && sweepFactor <= 1.0);

  @override
  void paint(Canvas canvas, Size size) {
    final center = Offset(size.width / 2, size.height / 2);
    final radius = size.width / 2;

    // --- Configuration des angles ---
    final totalSweepAngle = 2 * pi * sweepFactor;
    final gapAngle = 2 * pi * (1 - sweepFactor);
    // On centre le "trou" en bas. On commence donc à dessiner après la moitié du trou.
    // L'angle 0 est à droite (3h), pi/2 est en bas (6h).
    final startAngle = (pi / 2) + (gapAngle / 2);

    // --- Configuration des peintures ---
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

    // --- Dessin sur le Canvas ---
    final rect = Rect.fromCircle(center: center, radius: radius - strokeWidth / 2);
    canvas.drawArc(rect, startAngle, totalSweepAngle, false, backgroundPaint);
    final progressSweepAngle = totalSweepAngle * progress;
    canvas.drawArc(rect, startAngle, progressSweepAngle, false, foregroundPaint);
  }

  @override
  bool shouldRepaint(covariant MusicDial oldDelegate) {
    return progress != oldDelegate.progress ||
        backgroundColor != oldDelegate.backgroundColor ||
        foregroundColor != oldDelegate.foregroundColor ||
        strokeWidth != oldDelegate.strokeWidth ||
        sweepFactor != oldDelegate.sweepFactor;
  }
}
