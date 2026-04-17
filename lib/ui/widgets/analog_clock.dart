import 'dart:math';
import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/ui/themes/clock_theme.dart';

/// Horloge analogique à deux couches de rendu.
///
/// - [_ClockFacePainter] — cadran statique (ticks, points, chiffres).
///   Enveloppé dans [RepaintBoundary] : rasterisé une fois en texture GPU,
///   jamais repeint sauf si le thème change.
/// - [_ClockHandsPainter] — aiguilles, ombres et pivot central.
///   Repeint une fois par minute (shouldRepaint compare heure + minute).
///
/// Tous les objets [Paint] sont créés dans les constructeurs des painters —
/// aucune allocation dans les boucles paint().
class AnalogClock extends StatelessWidget {
  final DateTime dateTime;

  const AnalogClock({super.key, required this.dateTime});

  @override
  Widget build(BuildContext context) {
    final clockTheme = Theme.of(context).extension<AnalogClockTheme>()!;
    return Stack(
      fit: StackFit.expand,
      children: [
        // Static face — cached as GPU texture; only redrawn on theme change.
        RepaintBoundary(
          child: CustomPaint(
            painter: _ClockFacePainter(theme: clockTheme),
          ),
        ),
        // Dynamic layer — hands + shadows + pivot; repaints once per minute.
        CustomPaint(
          painter: _ClockHandsPainter(dateTime: dateTime, theme: clockTheme),
        ),
      ],
    );
  }
}

// ── Static face painter ───────────────────────────────────────────────────────

/// Draws hour ticks, minute dots, and number labels.
///
/// [Paint] objects are pre-allocated in the constructor.
/// Hour labels (12, 3, 6, 9) are cached in [_LabelCache] keyed by size.width —
/// layout() is only called when size changes, not on every paint.
class _ClockFacePainter extends CustomPainter {
  final AnalogClockTheme theme;

  final Paint _hourTickPaint;
  final Paint _minuteDotPaint;

  _ClockFacePainter({required this.theme})
      : _hourTickPaint = Paint()
          ..color = theme.hourTickColor ?? Colors.black
          ..strokeWidth = 2.0,
        _minuteDotPaint = Paint()
          ..color = theme.minuteDotColor ?? Colors.black54;

  @override
  void paint(Canvas canvas, Size size) {
    final centerX = size.width / 2;
    final centerY = size.height / 2;
    final radius = min(centerX, centerY);

    final labelCache = _LabelCache.get(size.width, radius, theme);

    for (int i = 0; i < 60; i++) {
      final angle = (i * 6 - 90) * (pi / 180);

      if (i % 5 == 0) {
        final hour = i ~/ 5;

        if (hour % 3 == 0) {
          // Major tick + number at 12, 3, 6, 9.
          canvas.drawLine(
            Offset(
              centerX + cos(angle) * (radius * 0.78),
              centerY + sin(angle) * (radius * 0.82),
            ),
            Offset(
              centerX + cos(angle) * (radius * 0.92),
              centerY + sin(angle) * (radius * 1),
            ),
            _hourTickPaint,
          );

          final labelIndex = hour ~/ 3; // 0=12, 1=3, 2=6, 3=9
          final cached = labelCache.labels[labelIndex];
          cached.painter.paint(
            canvas,
            Offset(
              centerX + cos(angle) * (radius * 0.70) - cached.halfWidth,
              centerY + sin(angle) * (radius * 0.70) - cached.halfHeight,
            ),
          );
        } else {
          // Minor hour tick (no number).
          canvas.drawLine(
            Offset(
              centerX + cos(angle) * (radius * 0.82),
              centerY + sin(angle) * (radius * 0.82),
            ),
            Offset(
              centerX + cos(angle) * (radius * 0.92),
              centerY + sin(angle) * (radius * 0.92),
            ),
            _hourTickPaint,
          );
        }
      } else {
        // Minute dot.
        canvas.drawCircle(
          Offset(
            centerX + cos(angle) * (radius * 0.87),
            centerY + sin(angle) * (radius * 0.87),
          ),
          1.0,
          _minuteDotPaint,
        );
      }
    }
  }

  @override
  bool shouldRepaint(covariant _ClockFacePainter old) => theme != old.theme;
}

/// Cached pre-laid-out TextPainters for the 4 cardinal hour labels.
class _CachedLabel {
  final TextPainter painter;
  final double halfWidth;
  final double halfHeight;

  _CachedLabel(this.painter)
      : halfWidth = painter.width / 2,
        halfHeight = painter.height / 2;
}

/// Memoized cache for hour label layouts, keyed by size.width.
class _LabelCache {
  static double? _cachedWidth;
  static Color? _cachedColor;
  static _LabelCache? _instance;

  static const _cardinalLabels = ['12', '3', '6', '9'];

  final List<_CachedLabel> labels;

  _LabelCache._(this.labels);

  static _LabelCache get(double width, double radius, AnalogClockTheme theme) {
    final textColor = theme.numberColor ?? Colors.black;
    if (_instance != null && _cachedWidth == width && _cachedColor == textColor) {
      return _instance!;
    }

    final textStyle = TextStyle(
      color: textColor,
      fontSize: radius * 0.18,
      fontWeight: FontWeight.w600,
    );

    final labels = _cardinalLabels.map((text) {
      final painter = TextPainter(
        text: TextSpan(text: text, style: textStyle),
        textAlign: TextAlign.center,
        textDirection: TextDirection.ltr,
      )..layout();
      return _CachedLabel(painter);
    }).toList();

    _cachedWidth = width;
    _cachedColor = textColor;
    _instance = _LabelCache._(labels);
    return _instance!;
  }
}

// ── Dynamic hands painter ─────────────────────────────────────────────────────

/// Draws hour hand, minute hand, drop shadows, and center pivot.
///
/// The pivot is drawn last so it renders on top of both hands.
/// All [Paint] objects are pre-allocated in the constructor.
/// Only repaints when [dateTime.hour] or [dateTime.minute] changes.
class _ClockHandsPainter extends CustomPainter {
  final DateTime dateTime;
  final AnalogClockTheme theme;

  final Paint _handPaint;
  final Paint _shadowPaint;
  final Paint _pivotPaint;
  final Paint _ridgesPaint; // strokeWidth set per paint call (size-dependent)

  _ClockHandsPainter({required this.dateTime, required this.theme})
      : _handPaint = Paint()
          ..color = theme.handColor ?? const Color.fromARGB(255, 184, 43, 43)
          ..style = PaintingStyle.fill,
        _shadowPaint = Paint()
          ..color = (theme.shadowColor ?? Colors.black.withAlpha(128))
          ..maskFilter = const MaskFilter.blur(BlurStyle.normal, 3.0),
        _pivotPaint = Paint()
          ..color = theme.centerPivotColor ?? Colors.black,
        _ridgesPaint = Paint()
          ..color = theme.centerPivotRidgeColor ?? Colors.grey.shade600
          ..style = PaintingStyle.stroke;

  @override
  void paint(Canvas canvas, Size size) {
    final centerX = size.width / 2;
    final centerY = size.height / 2;
    final center = Offset(centerX, centerY);
    final radius = min(centerX, centerY);
    final shadowOffset = Offset(radius * 0.015, radius * 0.015);

    final hourAngle =
        ((dateTime.hour % 12 + dateTime.minute / 60) * 30 - 90) * (pi / 180);
    final minuteAngle = (dateTime.minute * 6 - 90) * (pi / 180);

    final hourHandPath = _buildHandPath(
      centerX,
      centerY,
      hourAngle,
      radius * 0.55,
      radius * 0.1,
    );
    final minuteHandPath = _buildHandPath(
      centerX,
      centerY,
      minuteAngle,
      radius * 0.8,
      radius * 0.1,
    );

    // Shadows first (behind hands).
    canvas.drawPath(hourHandPath.shift(shadowOffset), _shadowPaint);
    canvas.drawPath(minuteHandPath.shift(shadowOffset), _shadowPaint);

    // Hands.
    canvas.drawPath(hourHandPath, _handPaint);
    canvas.drawPath(minuteHandPath, _handPaint);

    // Pivot — drawn last so it sits on top of both hands.
    final pivotRadius = radius * 0.12;
    canvas.drawCircle(center, pivotRadius, _pivotPaint);
    canvas.drawCircle(center, pivotRadius * 0.7, _pivotPaint);

    _ridgesPaint.strokeWidth = pivotRadius * 0.3; // size-derived, set here
    for (int i = 0; i < 12; i++) {
      final ridgeAngle = i * 30 * (pi / 180);
      canvas.drawLine(
        Offset(
          centerX + cos(ridgeAngle) * pivotRadius * 0.75,
          centerY + sin(ridgeAngle) * pivotRadius * 0.75,
        ),
        Offset(
          centerX + cos(ridgeAngle) * pivotRadius * 0.85,
          centerY + sin(ridgeAngle) * pivotRadius * 0.85,
        ),
        _ridgesPaint,
      );
    }
  }

  static Path _buildHandPath(
    double cx,
    double cy,
    double angle,
    double length,
    double width,
  ) {
    return Path()
      ..moveTo(
        cx - cos(angle + pi / 2) * width / 2,
        cy - sin(angle + pi / 2) * width / 2,
      )
      ..lineTo(cx + cos(angle) * length, cy + sin(angle) * length)
      ..lineTo(
        cx - cos(angle - pi / 2) * width / 2,
        cy - sin(angle - pi / 2) * width / 2,
      )
      ..close();
  }

  @override
  bool shouldRepaint(covariant _ClockHandsPainter old) =>
      dateTime.hour != old.dateTime.hour ||
      dateTime.minute != old.dateTime.minute ||
      theme != old.theme;
}
