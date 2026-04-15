import 'dart:math' as math;
import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/ui/themes/gauge_theme.dart';

/// Jauge circulaire animée avec segments.
///
/// Architecture deux couches:
/// - [_DialBackgroundPainter] — dessine uniquement les segments inactifs.
///   Enveloppé dans un [RepaintBoundary] : rasterisé en texture GPU et jamais
///   repeint entre deux changements de valeur ou de thème.
/// - [_DialActivePainter] — dessine uniquement les segments actifs/en cours.
///   Utilise l'animation comme notifier de repaint au niveau RenderObject ;
///   paint() est appelé à chaque tick sans rebuild de widget.
///
/// [build] n'est appelé qu'à 10 Hz (changement de valeur via Selector).
/// Le [Tween] et la [CurvedAnimation] sont créés une seule fois dans initState
/// et mutés dans didUpdateWidget — zéro allocation par tick d'animation.
class DigitalDial extends StatefulWidget {
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
  State<DigitalDial> createState() => _DigitalDialState();
}

class _DigitalDialState extends State<DigitalDial>
    with SingleTickerProviderStateMixin {
  late final AnimationController _controller;
  late final Tween<double> _tween;
  late final CurvedAnimation _curvedAnimation;
  late final Animation<double> _animation;

  @override
  void initState() {
    super.initState();
    _controller = AnimationController(
      duration: const Duration(milliseconds: 200),
      vsync: this,
    );
    _tween = Tween<double>(begin: 0, end: widget.value);
    _curvedAnimation = CurvedAnimation(
      parent: _controller,
      curve: Curves.easeInOutCubic,
    );
    _animation = _tween.animate(_curvedAnimation);
    _controller.forward();
  }

  @override
  void didUpdateWidget(DigitalDial oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (widget.value != oldWidget.value) {
      // Mutate the existing tween in-place — no allocation.
      _tween.begin = _animation.value;
      _tween.end = widget.value;
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

    final activeColor = widget.activeColor ?? gaugeTheme.activeColor!;
    final inactiveColor = widget.inactiveColor ?? gaugeTheme.inactiveColor!;
    final dangerColor = widget.dangerColor ?? gaugeTheme.dangerColor!;
    final dangerInactiveColor =
        widget.dangerInactiveColor ?? gaugeTheme.dangerInactiveColor!;

    // Two-layer stack:
    // 1. Background (inactive segments) wrapped in RepaintBoundary — GPU texture,
    //    only invalidated when static config or theme changes.
    // 2. Active segments driven by animation notifier at render layer.
    return Stack(
      fit: StackFit.expand,
      children: [
        RepaintBoundary(
          child: CustomPaint(
            painter: _DialBackgroundPainter(
              maxValue: widget.maxValue,
              numberOfSegments: widget.numberOfSegments,
              segmentHeight: widget.segmentHeight,
              segmentSpacing: widget.segmentSpacing,
              inactiveColor: inactiveColor,
              dangerThreshold: widget.dangerThreshold,
              dangerInactiveColor: dangerInactiveColor,
            ),
          ),
        ),
        CustomPaint(
          painter: _DialActivePainter(
            animation: _animation,
            maxValue: widget.maxValue,
            numberOfSegments: widget.numberOfSegments,
            segmentHeight: widget.segmentHeight,
            segmentSpacing: widget.segmentSpacing,
            activeColor: activeColor,
            dangerThreshold: widget.dangerThreshold,
            dangerColor: dangerColor,
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
/// [super(repaint: animation)] hooks the animation into the RenderObject:
/// [paint] is called at each animation tick without any widget rebuild.
/// Geometry and [Paint] objects are pre-calculated once at construction.
class _DialActivePainter extends CustomPainter {
  final Animation<double> animation;
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
    required this.animation,
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
         ..strokeCap = StrokeCap.butt,
       super(repaint: animation); // drives repaints at render layer

  @override
  void paint(Canvas canvas, Size size) {
    // Read animated value at paint time — not at build time.
    final value = animation.value;

    final center = Offset(size.width / 2, size.height / 2);
    final radius = math.min(size.width, size.height) / 2 - segmentHeight + 10;
    final rect = Rect.fromCircle(center: center, radius: radius);

    final progress = (value / maxValue).clamp(0.0, 1.0);
    final continuousSegments = progress * numberOfSegments;
    final fullSegments = continuousSegments.floor();
    final partialProgress = continuousSegments - fullSegments;

    for (int i = 0; i <= fullSegments && i < numberOfSegments; i++) {
      final segStart = _startAngle + i * (_segmentRadians + _gapInRadians);
      final paint = i >= _dangerSegmentStart
          ? _dangerActivePaint
          : _activePaint;

      if (i < fullSegments) {
        canvas.drawArc(rect, segStart, _segmentRadians, false, paint);
      } else {
        // Partial segment at the progress boundary.
        final partial = _segmentRadians * partialProgress;
        if (partial > 0) {
          canvas.drawArc(rect, segStart, partial, false, paint);
        }
      }
    }
  }

  @override
  bool shouldRepaint(covariant _DialActivePainter old) =>
      old.animation != animation ||
      old.maxValue != maxValue ||
      old.numberOfSegments != numberOfSegments ||
      old.segmentHeight != segmentHeight ||
      old.segmentSpacing != segmentSpacing ||
      old.dangerThreshold != dangerThreshold ||
      old._activePaint.color != _activePaint.color ||
      old._dangerActivePaint.color != _dangerActivePaint.color;
}
