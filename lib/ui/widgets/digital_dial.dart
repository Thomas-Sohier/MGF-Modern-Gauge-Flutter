import 'dart:math' as math;
import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/ui/themes/gauge_theme.dart';

class DigitalDial extends StatefulWidget {
  final double value;
  final double maxValue;
  final String unit;
  final Widget? child;
  final int numberOfSegments;
  final double segmentHeight;
  final double segmentSpacing;
  final Color? activeColor;
  final Color? inactiveColor;
  final double? dangerThreshold;
  final Color? dangerColor;
  final Color? dangerInactiveColor;
  final bool showGaugeBorder;
  final Color? gaugeBorderColor;
  final double gaugeBorderWidth;
  final double gaugeBorderSpacing;
  final List<Widget>? bottomChildren;
  final double bottomChildrenRadiusFactor;

  const DigitalDial({
    super.key,
    required this.value,
    required this.maxValue,
    this.unit = '',
    this.child,
    this.numberOfSegments = 12,
    this.segmentHeight = 20.0,
    this.segmentSpacing = 3.0,
    this.activeColor,
    this.inactiveColor,
    this.dangerThreshold,
    this.dangerColor,
    this.dangerInactiveColor,
    this.showGaugeBorder = false,
    this.gaugeBorderColor,
    this.gaugeBorderWidth = 1.0,
    this.gaugeBorderSpacing = 4.0,
    this.bottomChildren,
    this.bottomChildrenRadiusFactor = 0.65,
  });

  @override
  State<DigitalDial> createState() => _DigitalDialState();
}

class _DigitalDialState extends State<DigitalDial> with SingleTickerProviderStateMixin {
  late AnimationController _controller;
  late Animation<double> _animation;

  // ... (initState, didUpdateWidget, dispose restent identiques)
  @override
  void initState() {
    super.initState();
    _controller = AnimationController(duration: const Duration(milliseconds: 400), vsync: this);
    _animation = Tween<double>(
      begin: 0,
      end: widget.value,
    ).animate(CurvedAnimation(parent: _controller, curve: Curves.easeInOutCubic));
    _controller.forward();
  }

  @override
  void didUpdateWidget(DigitalDial oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (widget.value != oldWidget.value) {
      _animation = Tween<double>(
        begin: _animation.value,
        end: widget.value,
      ).animate(CurvedAnimation(parent: _controller, curve: Curves.easeInOutCubic));
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
    final finalActiveColor = widget.activeColor ?? gaugeTheme.activeColor!;
    final finalInactiveColor = widget.inactiveColor ?? gaugeTheme.inactiveColor!;
    final finalDangerColor = widget.dangerColor ?? gaugeTheme.dangerColor!;
    final finalDangerInactiveColor = widget.dangerInactiveColor ?? gaugeTheme.dangerInactiveColor!;
    final finalBorderColor = widget.gaugeBorderColor ?? gaugeTheme.borderColor!;

    return LayoutBuilder(
      builder: (context, constraints) {
        final size = constraints.biggest;
        final center = Offset(size.width / 2, size.height / 2);
        final radius = math.min(size.width, size.height) / 2;

        return Stack(
          children: [
            Positioned.fill(
              child: AnimatedBuilder(
                animation: _animation,
                builder: (context, child) {
                  return CustomPaint(
                    painter: _DialPainter(
                      value: _animation.value,
                      maxValue: widget.maxValue,
                      numberOfSegments: widget.numberOfSegments,
                      segmentHeight: widget.segmentHeight,
                      segmentSpacing: widget.segmentSpacing,
                      activeColor: finalActiveColor,
                      inactiveColor: finalInactiveColor,
                      dangerThreshold: widget.dangerThreshold,
                      dangerColor: finalDangerColor,
                      dangerInactiveColor: finalDangerInactiveColor,
                      showGaugeBorder: widget.showGaugeBorder,
                      gaugeBorderColor: finalBorderColor,
                      gaugeBorderWidth: widget.gaugeBorderWidth,
                      gaugeBorderSpacing: widget.gaugeBorderSpacing,
                    ),
                    child: Center(child: widget.child ?? _buildDefaultChild(finalActiveColor, finalDangerColor)),
                  );
                },
              ),
            ),
            if (widget.bottomChildren != null && widget.bottomChildren!.isNotEmpty)
              ..._buildBottomChildrenLayout(center, radius),
          ],
        );
      },
    );
  }

  List<Widget> _buildBottomChildrenLayout(Offset center, double radius) {
    final children = widget.bottomChildren!;
    final count = children.length;
    const double startAngle = math.pi * 0.15;
    const double sweepAngle = math.pi * 0.7;
    return List.generate(count, (index) {
      double angle;
      if (count == 1) {
        angle = startAngle + sweepAngle / 2;
      } else {
        final angleStep = sweepAngle / (count - 1);
        angle = startAngle + index * angleStep;
      }
      final childRadius = radius * widget.bottomChildrenRadiusFactor;
      final x = center.dx + childRadius * math.cos(angle);
      final y = center.dy + childRadius * math.sin(angle);
      return Positioned(
        left: x,
        top: y,
        child: Transform.translate(offset: const Offset(-30, -20), child: children[index]),
      );
    });
  }

  Widget _buildDefaultChild(Color activeColor, Color dangerColor) {
    bool isDanger = widget.dangerThreshold != null && widget.value >= widget.dangerThreshold!;
    Color valueColor = isDanger ? dangerColor : activeColor;

    final digitalTextStyle = TextStyle(
      fontFamily: 'monospace',
      fontSize: 45,
      color: valueColor,
      fontWeight: FontWeight.bold,
    );
    final unitTextStyle = TextStyle(
      fontFamily: 'monospace',
      fontSize: 20,
      color: valueColor,
      fontWeight: FontWeight.w500,
    );
    return Column(
      mainAxisAlignment: MainAxisAlignment.center,
      children: [
        Text(widget.value.round().toString(), style: digitalTextStyle),
        const SizedBox(height: 4),
        Text(widget.unit, style: unitTextStyle),
      ],
    );
  }
}

// Le _DialPainter reste identique, il reçoit juste les couleurs à utiliser.
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
  final bool showGaugeBorder;
  final Color gaugeBorderColor;
  final double gaugeBorderWidth;
  final double gaugeBorderSpacing;

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
    required this.showGaugeBorder,
    required this.gaugeBorderColor,
    required this.gaugeBorderWidth,
    required this.gaugeBorderSpacing,
  });

  // Le code de `paint` et `shouldRepaint` reste le même.
  @override
  void paint(Canvas canvas, Size size) {
    final center = Offset(size.width / 2, size.height / 2);
    final radius = math.min(size.width, size.height) / 2 - segmentHeight + 10;
    const startAngle = math.pi;
    const sweepAngle = math.pi;
    if (showGaugeBorder) {
      final borderPaint = Paint()
        ..color = gaugeBorderColor
        ..style = PaintingStyle.stroke
        ..strokeWidth = gaugeBorderWidth;
      final outerBorderRadius = radius + segmentHeight / 2 + gaugeBorderSpacing;
      canvas.drawCircle(center, outerBorderRadius, borderPaint);
    }
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
      final currentStartAngle = startAngle + i * (segmentRadians + gapInRadians);
      final bool isInDangerZone = i >= dangerSegmentStart;
      final Color currentActiveColor = isInDangerZone ? dangerColor : activeColor;
      final Color currentInactiveColor = isInDangerZone ? dangerInactiveColor : inactiveColor;
      if (i < fullSegments) {
        final segmentPaint = Paint()
          ..color = currentActiveColor
          ..style = PaintingStyle.stroke
          ..strokeWidth = segmentHeight
          ..strokeCap = StrokeCap.butt;
        canvas.drawArc(
          Rect.fromCircle(center: center, radius: radius),
          currentStartAngle,
          segmentRadians,
          false,
          segmentPaint,
        );
      } else if (i == fullSegments) {
        final inactivePaint = Paint()
          ..color = currentInactiveColor
          ..style = PaintingStyle.stroke
          ..strokeWidth = segmentHeight
          ..strokeCap = StrokeCap.butt;
        canvas.drawArc(
          Rect.fromCircle(center: center, radius: radius),
          currentStartAngle,
          segmentRadians,
          false,
          inactivePaint,
        );
        final activePaint = Paint()
          ..color = currentActiveColor
          ..style = PaintingStyle.stroke
          ..strokeWidth = segmentHeight
          ..strokeCap = StrokeCap.butt;
        final partialSweepAngle = segmentRadians * partialSegmentProgress;
        canvas.drawArc(
          Rect.fromCircle(center: center, radius: radius),
          currentStartAngle,
          partialSweepAngle,
          false,
          activePaint,
        );
      } else {
        final segmentPaint = Paint()
          ..color = currentInactiveColor
          ..style = PaintingStyle.stroke
          ..strokeWidth = segmentHeight
          ..strokeCap = StrokeCap.butt;
        canvas.drawArc(
          Rect.fromCircle(center: center, radius: radius),
          currentStartAngle,
          segmentRadians,
          false,
          segmentPaint,
        );
      }
    }
  }

  @override
  bool shouldRepaint(covariant _DialPainter oldDelegate) {
    return oldDelegate.value != value ||
        oldDelegate.maxValue != maxValue ||
        oldDelegate.segmentHeight != segmentHeight ||
        oldDelegate.activeColor != activeColor ||
        oldDelegate.inactiveColor != inactiveColor ||
        oldDelegate.dangerThreshold != dangerThreshold ||
        oldDelegate.dangerColor != dangerColor ||
        oldDelegate.showGaugeBorder != showGaugeBorder ||
        oldDelegate.gaugeBorderSpacing != gaugeBorderSpacing ||
        oldDelegate.dangerInactiveColor != dangerInactiveColor;
  }
}
