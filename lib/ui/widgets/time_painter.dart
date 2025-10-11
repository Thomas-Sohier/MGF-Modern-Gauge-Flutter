import 'dart:math';
import 'package:flutter/material.dart';

class AnalogClockPainter extends CustomPainter {
  final DateTime dateTime;

  AnalogClockPainter({required this.dateTime});

  @override
  void paint(Canvas canvas, Size size) {
    final centerX = size.width / 2;
    final centerY = size.height / 2;
    final center = Offset(centerX, centerY);
    final radius = min(centerX, centerY);

    // --- Configuration des ombres (inchangé) ---
    final shadowPaint = Paint()
      ..color = Colors.black.withOpacity(0.5)
      ..maskFilter = const MaskFilter.blur(BlurStyle.normal, 3.0);
    final shadowOffset = Offset(radius * 0.015, radius * 0.015);

    // --- 2. Dessin des marqueurs et des chiffres (logique modifiée) ---
    final textPainter = TextPainter(textAlign: TextAlign.center, textDirection: TextDirection.ltr);
    final hourTickPaint = Paint()
      ..color = Colors.black
      ..strokeWidth = 2.0;

    for (int i = 0; i < 60; i++) {
      final angle = (i * 6 - 90) * (pi / 180);
      if (i % 5 == 0) {
        final hour = (i / 5).toInt();

        if (hour % 3 == 0) {
          final tickStart = Offset(centerX + cos(angle) * (radius * 0.78), centerY + sin(angle) * (radius * 0.82));
          final tickEnd = Offset(centerX + cos(angle) * (radius * 0.92), centerY + sin(angle) * (radius * 1));
          canvas.drawLine(tickStart, tickEnd, hourTickPaint);

          final numberToDisplay = hour == 0 ? '12' : '$hour';
          textPainter.text = TextSpan(
            text: numberToDisplay,
            style: TextStyle(color: Colors.black, fontSize: radius * 0.18, fontWeight: FontWeight.w600),
          );
          textPainter.layout();
          final textOffset = Offset(
            centerX + cos(angle) * (radius * 0.70) - textPainter.width / 2,
            centerY + sin(angle) * (radius * 0.70) - textPainter.height / 2,
          );
          textPainter.paint(canvas, textOffset);
        } else {
          // Marqueurs courts pour les autres heures.
          final tickStart = Offset(centerX + cos(angle) * (radius * 0.82), centerY + sin(angle) * (radius * 0.82));
          final tickEnd = Offset(centerX + cos(angle) * (radius * 0.92), centerY + sin(angle) * (radius * 0.92));
          canvas.drawLine(tickStart, tickEnd, hourTickPaint);
        }
      } else {
        // Marqueurs de minutes (points)
        final dotPaint = Paint()..color = Colors.black54;
        final dotPosition = Offset(centerX + cos(angle) * (radius * 0.87), centerY + sin(angle) * (radius * 0.87));
        canvas.drawCircle(dotPosition, 1.0, dotPaint);
      }
    }

    // --- 3. Dessin des aiguilles (inchangé) ---
    final handPaint = Paint()
      ..color = const Color.fromARGB(255, 184, 43, 43)
      ..style = PaintingStyle.fill;

    final hourAngle = ((dateTime.hour % 12 + dateTime.minute / 60) * 30 - 90) * (pi / 180);
    final minuteAngle = (dateTime.minute * 6 - 90) * (pi / 180);

    final hourHandPath = Path();
    final hourHandLength = radius * 0.55;
    final hourHandWidth = radius * 0.1;
    hourHandPath.moveTo(
      centerX - cos(hourAngle + pi / 2) * hourHandWidth / 2,
      centerY - sin(hourAngle + pi / 2) * hourHandWidth / 2,
    );
    hourHandPath.lineTo(centerX + cos(hourAngle) * hourHandLength, centerY + sin(hourAngle) * hourHandLength);
    hourHandPath.lineTo(
      centerX - cos(hourAngle - pi / 2) * hourHandWidth / 2,
      centerY - sin(hourAngle - pi / 2) * hourHandWidth / 2,
    );
    hourHandPath.close();

    final minuteHandPath = Path();
    final minuteHandLength = radius * 0.8;
    final minuteHandWidth = radius * 0.1;
    minuteHandPath.moveTo(
      centerX - cos(minuteAngle + pi / 2) * minuteHandWidth / 2,
      centerY - sin(minuteAngle + pi / 2) * minuteHandWidth / 2,
    );
    minuteHandPath.lineTo(centerX + cos(minuteAngle) * minuteHandLength, centerY + sin(minuteAngle) * minuteHandLength);
    minuteHandPath.lineTo(
      centerX - cos(minuteAngle - pi / 2) * minuteHandWidth / 2,
      centerY - sin(minuteAngle - pi / 2) * minuteHandWidth / 2,
    );
    minuteHandPath.close();

    canvas.drawPath(hourHandPath.shift(shadowOffset), shadowPaint);
    canvas.drawPath(minuteHandPath.shift(shadowOffset), shadowPaint);

    canvas.drawPath(hourHandPath, handPaint);
    canvas.drawPath(minuteHandPath, handPaint);

    // --- 4. Dessin du pivot central (inchangé) ---
    final pivotRadius = radius * 0.12;
    // J'ai ré-ajouté la ligne manquante pour le fond du pivot
    canvas.drawCircle(center, pivotRadius, Paint()..color = Colors.black);
    canvas.drawCircle(center, pivotRadius * 0.7, Paint()..color = Colors.black);
    final ridgesPaint = Paint()
      ..color = Colors.grey.shade600
      ..style = PaintingStyle.stroke
      ..strokeWidth = pivotRadius * 0.3;
    for (int i = 0; i < 12; i++) {
      final ridgeAngle = i * 30 * (pi / 180);
      final ridgeStart = Offset(
        centerX + cos(ridgeAngle) * pivotRadius * 0.75,
        centerY + sin(ridgeAngle) * pivotRadius * 0.75,
      );
      final ridgeEnd = Offset(
        centerX + cos(ridgeAngle) * pivotRadius * 0.85,
        centerY + sin(ridgeAngle) * pivotRadius * 0.85,
      );
      canvas.drawLine(ridgeStart, ridgeEnd, ridgesPaint);
    }
  }

  @override
  bool shouldRepaint(covariant AnalogClockPainter oldDelegate) {
    return dateTime != oldDelegate.dateTime;
  }
}
