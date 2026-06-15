import 'dart:math' as math;
import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/ui/themes/gauge_theme.dart';

/// Dual-arc gauge combining throttle (inner) and primary (outer) arcs.
///
/// Draws both arcs in a single paint pass, reducing GPU overdraw vs two
/// separate DigitalDial widgets. Uses two-layer architecture:
/// - [_DualArcBackgroundPainter] — inactive segments, wrapped in RepaintBoundary
/// - [_DualArcActivePainter] — active segments, wrapped in RepaintBoundary,
///   repaints only when a value changes.
///
/// No animation: data arrives at ~10 Hz, so a transition tween adds nothing
/// visible and would force per-frame repaints.
class DualArcDial extends StatelessWidget {
  final double throttleValue;
  final double throttleMaxValue;
  final double primaryValue;
  final double primaryMaxValue;
  final double? primaryDangerThreshold;

  const DualArcDial({
    super.key,
    required this.throttleValue,
    this.throttleMaxValue = 100,
    required this.primaryValue,
    required this.primaryMaxValue,
    this.primaryDangerThreshold,
  });

  @override
  Widget build(BuildContext context) {
    final gaugeTheme = Theme.of(context).extension<GaugeTheme>()!;

    final activeColor = gaugeTheme.activeColor!;
    final inactiveColor = gaugeTheme.inactiveColor!;
    final dangerColor = gaugeTheme.dangerColor!;
    final dangerInactiveColor = gaugeTheme.dangerInactiveColor!;

    return Stack(
      fit: StackFit.expand,
      children: [
        RepaintBoundary(
          child: CustomPaint(
            painter: _DualArcBackgroundPainter(
              throttleMaxValue: throttleMaxValue,
              primaryMaxValue: primaryMaxValue,
              primaryDangerThreshold: primaryDangerThreshold,
              inactiveColor: inactiveColor,
              dangerInactiveColor: dangerInactiveColor,
            ),
          ),
        ),
        RepaintBoundary(
          child: CustomPaint(
            painter: _DualArcActivePainter(
              throttleValue: throttleValue,
              primaryValue: primaryValue,
              throttleMaxValue: throttleMaxValue,
              primaryMaxValue: primaryMaxValue,
              primaryDangerThreshold: primaryDangerThreshold,
              activeColor: activeColor,
              dangerColor: dangerColor,
            ),
          ),
        ),
      ],
    );
  }
}

// ── Constants ─────────────────────────────────────────────────────────────────

const double _startAngle = math.pi;
const double _sweepAngle = math.pi;

const double _throttleSegmentHeight = 12.0;
const double _throttleRadiusFactor = 0.7;

const int _primarySegments = 20;
const double _primarySegmentHeight = 40.0;
const double _primarySegmentSpacing = 3.0;

double _calcGapRadians(double spacing) => (spacing * math.pi) / 180;

double _calcSegmentRadians(double gapRadians, int segments) {
  final totalGap = gapRadians * (segments - 1);
  return (_sweepAngle - totalGap) / segments;
}

int _calcDangerStart(double? threshold, double maxValue, int segments) {
  if (threshold == null) return segments + 1;
  return ((threshold / maxValue) * segments).floor();
}

// ── Background painter ────────────────────────────────────────────────────────

class _DualArcBackgroundPainter extends CustomPainter {
  final double throttleMaxValue;
  final double primaryMaxValue;
  final double? primaryDangerThreshold;

  final double _primaryGapRadians;
  final double _primarySegmentRadians;
  final int _primaryDangerStart;

  final Paint _inactivePaint;
  final Paint _dangerInactivePaint;
  final Paint _throttleInactivePaint;

  _DualArcBackgroundPainter({
    required this.throttleMaxValue,
    required this.primaryMaxValue,
    required this.primaryDangerThreshold,
    required Color inactiveColor,
    required Color dangerInactiveColor,
  }) : _primaryGapRadians = _calcGapRadians(_primarySegmentSpacing),
       _primarySegmentRadians = _calcSegmentRadians(
         _calcGapRadians(_primarySegmentSpacing),
         _primarySegments,
       ),
       _primaryDangerStart = _calcDangerStart(
         primaryDangerThreshold,
         primaryMaxValue,
         _primarySegments,
       ),
       _inactivePaint = Paint()
         ..color = inactiveColor
         ..style = PaintingStyle.stroke
         ..strokeWidth = _primarySegmentHeight
         ..strokeCap = StrokeCap.butt,
       _dangerInactivePaint = Paint()
         ..color = dangerInactiveColor
         ..style = PaintingStyle.stroke
         ..strokeWidth = _primarySegmentHeight
         ..strokeCap = StrokeCap.butt,
       _throttleInactivePaint = Paint()
         ..color = inactiveColor
         ..style = PaintingStyle.stroke
         ..strokeWidth = _throttleSegmentHeight
         ..strokeCap = StrokeCap.butt;

  @override
  void paint(Canvas canvas, Size size) {
    final center = Offset(size.width / 2, size.height / 2);
    final baseRadius = math.min(size.width, size.height) / 2;

    // Outer primary arc (20 segments)
    final primaryRadius = baseRadius - _primarySegmentHeight + 10;
    final primaryRect = Rect.fromCircle(center: center, radius: primaryRadius);
    for (int i = 0; i < _primarySegments; i++) {
      final segStart =
          _startAngle + i * (_primarySegmentRadians + _primaryGapRadians);
      final paint = i >= _primaryDangerStart
          ? _dangerInactivePaint
          : _inactivePaint;
      canvas.drawArc(
        primaryRect,
        segStart,
        _primarySegmentRadians,
        false,
        paint,
      );
    }

    // Inner throttle arc (1 segment)
    final throttleRadius =
        (baseRadius - _primarySegmentHeight) * _throttleRadiusFactor;
    final throttleRect = Rect.fromCircle(
      center: center,
      radius: throttleRadius,
    );
    canvas.drawArc(
      throttleRect,
      _startAngle,
      _sweepAngle,
      false,
      _throttleInactivePaint,
    );
  }

  @override
  bool shouldRepaint(covariant _DualArcBackgroundPainter old) =>
      old.primaryMaxValue != primaryMaxValue ||
      old.primaryDangerThreshold != primaryDangerThreshold ||
      old._inactivePaint.color != _inactivePaint.color ||
      old._dangerInactivePaint.color != _dangerInactivePaint.color;
}

// ── Active painter ────────────────────────────────────────────────────────────

class _DualArcActivePainter extends CustomPainter {
  final double throttleValue;
  final double primaryValue;
  final double throttleMaxValue;
  final double primaryMaxValue;
  final double? primaryDangerThreshold;

  final double _primaryGapRadians;
  final double _primarySegmentRadians;
  final int _primaryDangerStart;

  final Paint _activePaint;
  final Paint _dangerActivePaint;
  final Paint _throttleActivePaint;

  _DualArcActivePainter({
    required this.throttleValue,
    required this.primaryValue,
    required this.throttleMaxValue,
    required this.primaryMaxValue,
    required this.primaryDangerThreshold,
    required Color activeColor,
    required Color dangerColor,
  }) : _primaryGapRadians = _calcGapRadians(_primarySegmentSpacing),
       _primarySegmentRadians = _calcSegmentRadians(
         _calcGapRadians(_primarySegmentSpacing),
         _primarySegments,
       ),
       _primaryDangerStart = _calcDangerStart(
         primaryDangerThreshold,
         primaryMaxValue,
         _primarySegments,
       ),
       _activePaint = Paint()
         ..color = activeColor
         ..style = PaintingStyle.stroke
         ..strokeWidth = _primarySegmentHeight
         ..strokeCap = StrokeCap.butt,
       _dangerActivePaint = Paint()
         ..color = dangerColor
         ..style = PaintingStyle.stroke
         ..strokeWidth = _primarySegmentHeight
         ..strokeCap = StrokeCap.butt,
       _throttleActivePaint = Paint()
         ..color = activeColor
         ..style = PaintingStyle.stroke
         ..strokeWidth = _throttleSegmentHeight
         ..strokeCap = StrokeCap.butt;

  @override
  void paint(Canvas canvas, Size size) {
    final center = Offset(size.width / 2, size.height / 2);
    final baseRadius = math.min(size.width, size.height) / 2;

    // Outer primary arc
    final primaryRadius = baseRadius - _primarySegmentHeight + 10;
    final primaryRect = Rect.fromCircle(center: center, radius: primaryRadius);
    final primaryProgress = (primaryValue / primaryMaxValue).clamp(0.0, 1.0);
    final continuousSegments = primaryProgress * _primarySegments;
    final fullSegments = continuousSegments.floor();
    final partialProgress = continuousSegments - fullSegments;

    // Batch same-color segments into one Path each — a single drawPath per
    // color instead of one drawArc per segment.
    final activePath = Path();
    final dangerPath = Path();

    for (int i = 0; i <= fullSegments && i < _primarySegments; i++) {
      final segStart =
          _startAngle + i * (_primarySegmentRadians + _primaryGapRadians);
      final path = i >= _primaryDangerStart ? dangerPath : activePath;

      if (i < fullSegments) {
        path.addArc(primaryRect, segStart, _primarySegmentRadians);
      } else {
        final partial = _primarySegmentRadians * partialProgress;
        if (partial > 0) {
          path.addArc(primaryRect, segStart, partial);
        }
      }
    }

    canvas.drawPath(activePath, _activePaint);
    canvas.drawPath(dangerPath, _dangerActivePaint);

    // Inner throttle arc
    final throttleRadius =
        (baseRadius - _primarySegmentHeight) * _throttleRadiusFactor;
    final throttleRect = Rect.fromCircle(
      center: center,
      radius: throttleRadius,
    );
    final throttleProgress = (throttleValue / throttleMaxValue).clamp(0.0, 1.0);
    final throttleSweep = _sweepAngle * throttleProgress;
    if (throttleSweep > 0) {
      canvas.drawArc(
        throttleRect,
        _startAngle,
        throttleSweep,
        false,
        _throttleActivePaint,
      );
    }
  }

  @override
  bool shouldRepaint(covariant _DualArcActivePainter old) =>
      old.throttleValue != throttleValue ||
      old.primaryValue != primaryValue ||
      old.primaryMaxValue != primaryMaxValue ||
      old.primaryDangerThreshold != primaryDangerThreshold ||
      old._activePaint.color != _activePaint.color ||
      old._dangerActivePaint.color != _dangerActivePaint.color;
}
