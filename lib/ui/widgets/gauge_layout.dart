import 'dart:math' as math;
import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/ui/widgets/digital_dial.dart';

/// Layout composant une jauge avec un contenu central et des indicateurs en arc.
class GaugeLayout extends StatelessWidget {
  final double value;
  final double maxValue;
  final String? unit;
  final double? dangerThreshold;
  final Widget child;
  final List<Widget> bottomChildren;
  final double bottomChildrenRadiusFactor;

  const GaugeLayout({
    super.key,
    required this.value,
    required this.maxValue,
    required this.child,
    this.unit,
    this.dangerThreshold,
    this.bottomChildren = const [],
    this.bottomChildrenRadiusFactor = 0.75,
  });

  @override
  Widget build(BuildContext context) {
    return LayoutBuilder(
      builder: (context, constraints) {
        final size = constraints.biggest;
        final center = Offset(size.width / 2, size.height / 2);
        final radius = math.min(size.width, size.height) / 2;

        return Stack(
          children: [
            // Jauge
            Positioned.fill(
              child: DigitalDial(
                value: value,
                maxValue: maxValue,
                dangerThreshold: dangerThreshold,
              ),
            ),
            // Contenu central
            Center(
              child: SizedBox(
                width: size.width * 0.5,
                height: size.height * 0.5,
                child: child,
              ),
            ),
            // Indicateurs en arc
            if (bottomChildren.isNotEmpty)
              ..._buildBottomChildrenLayout(center, radius),
          ],
        );
      },
    );
  }

  List<Widget> _buildBottomChildrenLayout(Offset center, double radius) {
    final count = bottomChildren.length;
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
      final childRadius = radius * bottomChildrenRadiusFactor;
      final x = center.dx + childRadius * math.cos(angle);
      final y = center.dy + childRadius * math.sin(angle);

      return Positioned(
        left: x,
        top: y,
        child: FractionalTranslation(
          translation: const Offset(-0.5, -0.5),
          child: bottomChildren[index],
        ),
      );
    });
  }
}
