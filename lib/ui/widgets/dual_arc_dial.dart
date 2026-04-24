import 'dart:math' as math;
import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/ui/themes/gauge_theme.dart';

/// Dual-arc gauge combining throttle (inner) and primary (outer) arcs.
///
/// Draws both arcs in a single paint pass, reducing GPU overdraw vs two
/// separate DigitalDial widgets. Uses two-layer architecture:
/// - [_DualArcBackgroundPainter] — inactive segments, wrapped in RepaintBoundary
/// - [_DualArcActivePainter] — active segments, driven by animation
class DualArcDial extends StatefulWidget {
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
  State<DualArcDial> createState() => _DualArcDialState();
}

class _DualArcDialState extends State<DualArcDial>
    with SingleTickerProviderStateMixin {
  late final AnimationController _controller;
  late final Tween<double> _throttleTween;
  late final Tween<double> _primaryTween;
  late final CurvedAnimation _curvedAnimation;
  late final Animation<double> _throttleAnimation;
  late final Animation<double> _primaryAnimation;

  @override
  void initState() {
    super.initState();
    _controller = AnimationController(
      duration: const Duration(milliseconds: 200),
      vsync: this,
    );
    _throttleTween = Tween<double>(begin: 0, end: widget.throttleValue);
    _primaryTween = Tween<double>(begin: 0, end: widget.primaryValue);
    _curvedAnimation = CurvedAnimation(
      parent: _controller,
      curve: Curves.easeInOutCubic,
    );
    _throttleAnimation = _throttleTween.animate(_curvedAnimation);
    _primaryAnimation = _primaryTween.animate(_curvedAnimation);
    _controller.forward();
  }

  @override
  void didUpdateWidget(DualArcDial oldWidget) {
    super.didUpdateWidget(oldWidget);
    bool changed = false;
    if (widget.throttleValue != oldWidget.throttleValue) {
      _throttleTween.begin = _throttleAnimation.value;
      _throttleTween.end = widget.throttleValue;
      changed = true;
    }
    if (widget.primaryValue != oldWidget.primaryValue) {
      _primaryTween.begin = _primaryAnimation.value;
      _primaryTween.end = widget.primaryValue;
      changed = true;
    }
    if (changed) {
      _controller
        ..reset()
        ..forward();
    }
  }

  @override
  void dispose() {
    _curvedAnimation.dispose();
    _controller.dispose();
    super.dispose();
  }

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
              throttleMaxValue: widget.throttleMaxValue,
              primaryMaxValue: widget.primaryMaxValue,
              primaryDangerThreshold: widget.primaryDangerThreshold,
              inactiveColor: inactiveColor,
              dangerInactiveColor: dangerInactiveColor,
            ),
          ),
        ),
        RepaintBoundary(
          child: CustomPaint(
            painter: _DualArcActivePainter(
              repaint: _curvedAnimation,
              throttleAnimation: _throttleAnimation,
              primaryAnimation: _primaryAnimation,
              throttleMaxValue: widget.throttleMaxValue,
              primaryMaxValue: widget.primaryMaxValue,
              primaryDangerThreshold: widget.primaryDangerThreshold,
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
  final Animation<double> throttleAnimation;
  final Animation<double> primaryAnimation;
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
    required Listenable repaint,
    required this.throttleAnimation,
    required this.primaryAnimation,
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
         ..strokeCap = StrokeCap.butt,
       super(repaint: repaint);

  @override
  void paint(Canvas canvas, Size size) {
    final center = Offset(size.width / 2, size.height / 2);
    final baseRadius = math.min(size.width, size.height) / 2;

    // Outer primary arc
    final primaryValue = primaryAnimation.value;
    final primaryRadius = baseRadius - _primarySegmentHeight + 10;
    final primaryRect = Rect.fromCircle(center: center, radius: primaryRadius);
    final primaryProgress = (primaryValue / primaryMaxValue).clamp(0.0, 1.0);
    final continuousSegments = primaryProgress * _primarySegments;
    final fullSegments = continuousSegments.floor();
    final partialProgress = continuousSegments - fullSegments;

    for (int i = 0; i <= fullSegments && i < _primarySegments; i++) {
      final segStart =
          _startAngle + i * (_primarySegmentRadians + _primaryGapRadians);
      final paint = i >= _primaryDangerStart
          ? _dangerActivePaint
          : _activePaint;

      if (i < fullSegments) {
        canvas.drawArc(
          primaryRect,
          segStart,
          _primarySegmentRadians,
          false,
          paint,
        );
      } else {
        final partial = _primarySegmentRadians * partialProgress;
        if (partial > 0) {
          canvas.drawArc(primaryRect, segStart, partial, false, paint);
        }
      }
    }

    // Inner throttle arc
    final throttleValue = throttleAnimation.value;
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
      old.throttleAnimation != throttleAnimation ||
      old.primaryAnimation != primaryAnimation ||
      old.primaryMaxValue != primaryMaxValue ||
      old.primaryDangerThreshold != primaryDangerThreshold ||
      old._activePaint.color != _activePaint.color ||
      old._dangerActivePaint.color != _dangerActivePaint.color;
}
