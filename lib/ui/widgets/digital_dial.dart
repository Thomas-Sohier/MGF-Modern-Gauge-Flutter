import 'dart:math' as math;
import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/ui/themes/gauge_theme.dart';

/// Jauge circulaire à segments.
///
/// Architecture deux couches:
/// - [_DialBackgroundPainter] — dessine uniquement les segments inactifs.
///   Enveloppé dans un [RepaintBoundary] : rasterisé en texture GPU et jamais
///   repeint entre deux changements de valeur ou de thème.
/// - [_DialActivePainter] — dessine uniquement les segments actifs/en cours.
///   Enveloppé dans un [RepaintBoundary] : ne repeint que lorsque la valeur
///   change (shouldRepaint compare la valeur).
///
/// Aucune animation : les données arrivent à ~10 Hz, un tween de transition
/// n'apporterait rien de visible et provoquerait des repeints à chaque frame.
class DigitalDial extends StatelessWidget {
  final double value;
  final double maxValue;
  final int numberOfSegments;
  final double segmentHeight;
  final double segmentSpacing;
  final Color? activeColor;
  final Color? inactiveColor;
  final double? dangerThreshold;
  final Color? dangerColor;
  final Color? dangerInactiveColor;

  const DigitalDial({
    super.key,
    required this.value,
    required this.maxValue,
    this.numberOfSegments = 20,
    this.segmentHeight = 40.0,
    this.segmentSpacing = 3.0,
    this.activeColor,
    this.inactiveColor,
    this.dangerThreshold,
    this.dangerColor,
    this.dangerInactiveColor,
  });

  @override
  Widget build(BuildContext context) {
    final gaugeTheme = Theme.of(context).extension<GaugeTheme>()!;

    final active = activeColor ?? gaugeTheme.activeColor!;
    final inactive = inactiveColor ?? gaugeTheme.inactiveColor!;
    final danger = dangerColor ?? gaugeTheme.dangerColor!;
    final dangerInactive =
        dangerInactiveColor ?? gaugeTheme.dangerInactiveColor!;

    // Two-layer stack, both wrapped in RepaintBoundary:
    // 1. Background (inactive segments) — GPU texture, invalidated only on
    //    static config or theme change.
    // 2. Active segments — repaints only when the value changes.
    return Stack(
      fit: StackFit.expand,
      children: [
        RepaintBoundary(
          child: CustomPaint(
            painter: _DialBackgroundPainter(
              maxValue: maxValue,
              numberOfSegments: numberOfSegments,
              segmentHeight: segmentHeight,
              segmentSpacing: segmentSpacing,
              inactiveColor: inactive,
              dangerThreshold: dangerThreshold,
              dangerInactiveColor: dangerInactive,
            ),
          ),
        ),
        RepaintBoundary(
          child: CustomPaint(
            painter: _DialActivePainter(
              value: value,
              maxValue: maxValue,
              numberOfSegments: numberOfSegments,
              segmentHeight: segmentHeight,
              segmentSpacing: segmentSpacing,
              activeColor: active,
              dangerThreshold: dangerThreshold,
              dangerColor: danger,
            ),
          ),
        ),
      ],
    );
  }
}

// ── Shared geometry helpers ───────────────────────────────────────────────────

const double _startAngle = math.pi;
const double _sweepAngle = math.pi;

double _calcGapRadians(double segmentSpacing) =>
    (segmentSpacing * math.pi) / 180;

double _calcSegmentRadians(double gapInRadians, int numberOfSegments) {
  final totalGapRadians = gapInRadians * (numberOfSegments - 1);
  return (_sweepAngle - totalGapRadians) / numberOfSegments;
}

int _calcDangerSegmentStart(double? dangerThreshold, double maxValue, int n) {
  if (dangerThreshold == null) return n + 1;
  return ((dangerThreshold / maxValue) * n).floor();
}

// ── Background painter (inactive segments only) ───────────────────────────────

/// Draws all segments in their inactive color.
/// Pre-calculates geometry and [Paint] once at construction.
/// Wrapped in [RepaintBoundary] by the parent — rasterized as a GPU layer
/// and never redrawn unless static configuration changes.
class _DialBackgroundPainter extends CustomPainter {
  final double maxValue;
  final int numberOfSegments;
  final double segmentHeight;
  final double segmentSpacing;
  final double? dangerThreshold;

  // Pre-computed geometry
  final double _gapInRadians;
  final double _segmentRadians;
  final int _dangerSegmentStart;

  // Pre-allocated Paint objects
  final Paint _inactivePaint;
  final Paint _dangerInactivePaint;

  _DialBackgroundPainter({
    required this.maxValue,
    required this.numberOfSegments,
    required this.segmentHeight,
    required this.segmentSpacing,
    required Color inactiveColor,
    required this.dangerThreshold,
    required Color dangerInactiveColor,
  }) : _gapInRadians = _calcGapRadians(segmentSpacing),
       _segmentRadians = _calcSegmentRadians(
         _calcGapRadians(segmentSpacing),
         numberOfSegments,
       ),
       _dangerSegmentStart = _calcDangerSegmentStart(
         dangerThreshold,
         maxValue,
         numberOfSegments,
       ),
       _inactivePaint = Paint()
         ..color = inactiveColor
         ..style = PaintingStyle.stroke
         ..strokeWidth = segmentHeight
         ..strokeCap = StrokeCap.butt,
       _dangerInactivePaint = Paint()
         ..color = dangerInactiveColor
         ..style = PaintingStyle.stroke
         ..strokeWidth = segmentHeight
         ..strokeCap = StrokeCap.butt;

  @override
  void paint(Canvas canvas, Size size) {
    final center = Offset(size.width / 2, size.height / 2);
    final radius = math.min(size.width, size.height) / 2 - segmentHeight + 10;
    final rect = Rect.fromCircle(center: center, radius: radius);

    for (int i = 0; i < numberOfSegments; i++) {
      final segStart = _startAngle + i * (_segmentRadians + _gapInRadians);
      final paint = i >= _dangerSegmentStart
          ? _dangerInactivePaint
          : _inactivePaint;
      canvas.drawArc(rect, segStart, _segmentRadians, false, paint);
    }
  }

  @override
  bool shouldRepaint(covariant _DialBackgroundPainter old) =>
      old.maxValue != maxValue ||
      old.numberOfSegments != numberOfSegments ||
      old.segmentHeight != segmentHeight ||
      old.segmentSpacing != segmentSpacing ||
      old.dangerThreshold != dangerThreshold ||
      old._inactivePaint.color != _inactivePaint.color ||
      old._dangerInactivePaint.color != _dangerInactivePaint.color;
}

// ── Active painter (progress segments only) ───────────────────────────────────

/// Draws only the active/progress portion of the dial.
/// Geometry and [Paint] objects are pre-calculated once at construction.
/// [shouldRepaint] compares the value, so paint only runs when it changes.
class _DialActivePainter extends CustomPainter {
  final double value;
  final double maxValue;
  final int numberOfSegments;
  final double segmentHeight;
  final double segmentSpacing;
  final double? dangerThreshold;

  // Pre-computed geometry
  final double _gapInRadians;
  final double _segmentRadians;
  final int _dangerSegmentStart;

  // Pre-allocated Paint objects
  final Paint _activePaint;
  final Paint _dangerActivePaint;

  _DialActivePainter({
    required this.value,
    required this.maxValue,
    required this.numberOfSegments,
    required this.segmentHeight,
    required this.segmentSpacing,
    required Color activeColor,
    required this.dangerThreshold,
    required Color dangerColor,
  }) : _gapInRadians = _calcGapRadians(segmentSpacing),
       _segmentRadians = _calcSegmentRadians(
         _calcGapRadians(segmentSpacing),
         numberOfSegments,
       ),
       _dangerSegmentStart = _calcDangerSegmentStart(
         dangerThreshold,
         maxValue,
         numberOfSegments,
       ),
       _activePaint = Paint()
         ..color = activeColor
         ..style = PaintingStyle.stroke
         ..strokeWidth = segmentHeight
         ..strokeCap = StrokeCap.butt,
       _dangerActivePaint = Paint()
         ..color = dangerColor
         ..style = PaintingStyle.stroke
         ..strokeWidth = segmentHeight
         ..strokeCap = StrokeCap.butt;

  @override
  void paint(Canvas canvas, Size size) {
    final center = Offset(size.width / 2, size.height / 2);
    final radius = math.min(size.width, size.height) / 2 - segmentHeight + 10;
    final rect = Rect.fromCircle(center: center, radius: radius);

    final progress = (value / maxValue).clamp(0.0, 1.0);
    final continuousSegments = progress * numberOfSegments;
    final fullSegments = continuousSegments.floor();
    final partialProgress = continuousSegments - fullSegments;

    // Batch same-color segments into one Path each — a single drawPath per
    // color instead of one drawArc per segment. Each addArc is its own
    // sub-path, so stroking with StrokeCap.butt is visually identical.
    final activePath = Path();
    final dangerPath = Path();

    for (int i = 0; i <= fullSegments && i < numberOfSegments; i++) {
      final segStart = _startAngle + i * (_segmentRadians + _gapInRadians);
      final path = i >= _dangerSegmentStart ? dangerPath : activePath;

      if (i < fullSegments) {
        path.addArc(rect, segStart, _segmentRadians);
      } else {
        // Partial segment at the progress boundary.
        final partial = _segmentRadians * partialProgress;
        if (partial > 0) {
          path.addArc(rect, segStart, partial);
        }
      }
    }

    canvas.drawPath(activePath, _activePaint);
    canvas.drawPath(dangerPath, _dangerActivePaint);
  }

  @override
  bool shouldRepaint(covariant _DialActivePainter old) =>
      old.value != value ||
      old.maxValue != maxValue ||
      old.numberOfSegments != numberOfSegments ||
      old.segmentHeight != segmentHeight ||
      old.segmentSpacing != segmentSpacing ||
      old.dangerThreshold != dangerThreshold ||
      old._activePaint.color != _activePaint.color ||
      old._dangerActivePaint.color != _dangerActivePaint.color;
}
