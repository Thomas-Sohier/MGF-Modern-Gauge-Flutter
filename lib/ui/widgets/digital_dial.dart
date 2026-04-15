import 'dart:math' as math;
import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/ui/themes/gauge_theme.dart';

/// Jauge circulaire animée avec segments.
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
  late AnimationController _controller;
  late Animation<double> _animation;

  @override
  void initState() {
    super.initState();
    _controller = AnimationController(
      duration: const Duration(milliseconds: 200),
      vsync: this,
    );
    _animation = Tween<double>(begin: 0, end: widget.value).animate(
      CurvedAnimation(parent: _controller, curve: Curves.easeInOutCubic),
    );
    _controller.forward();
  }

  @override
  void didUpdateWidget(DigitalDial oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (widget.value != oldWidget.value) {
      _animation = Tween<double>(begin: _animation.value, end: widget.value)
          .animate(
            CurvedAnimation(parent: _controller, curve: Curves.easeInOutCubic),
          );
      _controller.reset();
      _controller.forward();
    }
  }

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final gaugeTheme = Theme.of(context).extension<GaugeTheme>()!;

    return AnimatedBuilder(
      animation: _animation,
      builder: (context, child) {
        return CustomPaint(
          painter: _DialPainter(
            value: _animation.value,
            maxValue: widget.maxValue,
            numberOfSegments: widget.numberOfSegments,
            segmentHeight: widget.segmentHeight,
            segmentSpacing: widget.segmentSpacing,
            activeColor: widget.activeColor ?? gaugeTheme.activeColor!,
            inactiveColor: widget.inactiveColor ?? gaugeTheme.inactiveColor!,
            dangerThreshold: widget.dangerThreshold,
            dangerColor: widget.dangerColor ?? gaugeTheme.dangerColor!,
            dangerInactiveColor:
                widget.dangerInactiveColor ?? gaugeTheme.dangerInactiveColor!,
          ),
        );
      },
    );
  }
}

class _DialPainter extends CustomPainter {
  final double value;
  final double maxValue;
  final int numberOfSegments;
  final double segmentHeight;
  final double segmentSpacing;
  final Color activeColor;
  final Color inactiveColor;
  final double? dangerThreshold;
  final Color dangerColor;
  final Color dangerInactiveColor;

  _DialPainter({
    required this.value,
    required this.maxValue,
    required this.numberOfSegments,
    required this.segmentHeight,
    required this.segmentSpacing,
    required this.activeColor,
    required this.inactiveColor,
    this.dangerThreshold,
    required this.dangerColor,
    required this.dangerInactiveColor,
  });

  @override
  void paint(Canvas canvas, Size size) {
    final center = Offset(size.width / 2, size.height / 2);
    final radius = math.min(size.width, size.height) / 2 - segmentHeight + 10;
    const startAngle = math.pi;
    const sweepAngle = math.pi;

    final double gapInRadians = (segmentSpacing * math.pi) / 180;
    final double totalGapRadians = gapInRadians * (numberOfSegments - 1);
    final double totalSegmentRadians = sweepAngle - totalGapRadians;
    final double segmentRadians = totalSegmentRadians / numberOfSegments;

    final progress = (value / maxValue).clamp(0.0, 1.0);
    final continuousSegments = progress * numberOfSegments;
    final fullSegments = continuousSegments.floor();
    final partialSegmentProgress = continuousSegments - fullSegments;

    final int dangerSegmentStart = dangerThreshold == null
        ? numberOfSegments + 1
        : ((dangerThreshold! / maxValue) * numberOfSegments).floor();

    for (int i = 0; i < numberOfSegments; i++) {
      final currentStartAngle =
          startAngle + i * (segmentRadians + gapInRadians);
      final bool isInDangerZone = i >= dangerSegmentStart;
      final Color currentActiveColor = isInDangerZone
          ? dangerColor
          : activeColor;
      final Color currentInactiveColor = isInDangerZone
          ? dangerInactiveColor
          : inactiveColor;

      if (i < fullSegments) {
        _drawSegment(
          canvas,
          center,
          radius,
          currentStartAngle,
          segmentRadians,
          currentActiveColor,
        );
      } else if (i == fullSegments) {
        // Segment partiel : fond inactif + partie active
        _drawSegment(
          canvas,
          center,
          radius,
          currentStartAngle,
          segmentRadians,
          currentInactiveColor,
        );
        _drawSegment(
          canvas,
          center,
          radius,
          currentStartAngle,
          segmentRadians * partialSegmentProgress,
          currentActiveColor,
        );
      } else {
        _drawSegment(
          canvas,
          center,
          radius,
          currentStartAngle,
          segmentRadians,
          currentInactiveColor,
        );
      }
    }
  }

  void _drawSegment(
    Canvas canvas,
    Offset center,
    double radius,
    double startAngle,
    double sweepAngle,
    Color color,
  ) {
    final paint = Paint()
      ..color = color
      ..style = PaintingStyle.stroke
      ..strokeWidth = segmentHeight
      ..strokeCap = StrokeCap.butt;
    canvas.drawArc(
      Rect.fromCircle(center: center, radius: radius),
      startAngle,
      sweepAngle,
      false,
      paint,
    );
  }

  @override
  bool shouldRepaint(covariant _DialPainter oldDelegate) {
    return oldDelegate.value != value ||
        oldDelegate.maxValue != maxValue ||
        oldDelegate.numberOfSegments != numberOfSegments ||
        oldDelegate.segmentHeight != segmentHeight ||
        oldDelegate.segmentSpacing != segmentSpacing ||
        oldDelegate.activeColor != activeColor ||
        oldDelegate.inactiveColor != inactiveColor ||
        oldDelegate.dangerThreshold != dangerThreshold ||
        oldDelegate.dangerColor != dangerColor ||
        oldDelegate.dangerInactiveColor != dangerInactiveColor;
  }
}
