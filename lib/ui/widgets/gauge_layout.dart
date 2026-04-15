import 'dart:math' as math;
import 'package:flutter/material.dart';

/// Layout composant une jauge avec un contenu central et des indicateurs en arc.
///
/// [backgroundDial] est rendu en arrière-plan (Positioned.fill).
/// Son état est géré en dehors de ce widget (ex. Selector + DigitalDial),
/// ce qui isole les rebuilds haute-fréquence du reste de l'arbre.
///
/// Les [bottomChildren] sont positionnés en arc via un [Flow] + [_ArcFlowDelegate].
/// Le délégué ne recalcule les coordonnées (sin/cos) que si la taille ou
/// [bottomChildrenRadiusFactor] change — le coût math est nul sur les frames
/// provoquées uniquement par un changement de valeur.
class GaugeLayout extends StatelessWidget {
  /// Widget de fond (typiquement un DigitalDial dans un Selector).
  final Widget backgroundDial;

  /// Contenu central affiché dans la moitié intérieure de la jauge.
  final Widget child;

  /// Indicateurs disposés en arc sous la jauge.
  final List<Widget> bottomChildren;

  /// Fraction du rayon à laquelle les indicateurs sont placés.
  final double bottomChildrenRadiusFactor;

  const GaugeLayout({
    super.key,
    required this.backgroundDial,
    required this.child,
    this.bottomChildren = const [],
    this.bottomChildrenRadiusFactor = 0.75,
  });

  @override
  Widget build(BuildContext context) {
    return Stack(
      children: [
        // Dial de fond — son état est isolé, ne provoque pas de rebuild ici.
        Positioned.fill(child: backgroundDial),

        // Contenu central : 50 % de la taille disponible.
        Center(
          child: FractionallySizedBox(
            widthFactor: 0.5,
            heightFactor: 0.5,
            child: child,
          ),
        ),

        // Indicateurs en arc via Flow — le délégué gère le positionnement.
        if (bottomChildren.isNotEmpty)
          Positioned.fill(
            child: Flow(
              clipBehavior: Clip.none,
              delegate: _ArcFlowDelegate(
                radiusFactor: bottomChildrenRadiusFactor,
              ),
              children: bottomChildren,
            ),
          ),
      ],
    );
  }
}

/// Délégué Flow qui positionne les enfants en arc via des matrices de transformation.
///
/// [paintChildren] est la seule méthode qui effectue le calcul sin/cos.
/// [shouldRepaint] retourne false tant que [radiusFactor] est inchangé,
/// évitant tout recalcul sur les rebuilds provoqués par des changements de valeur.
class _ArcFlowDelegate extends FlowDelegate {
  final double radiusFactor;

  const _ArcFlowDelegate({required this.radiusFactor});

  @override
  Size getSize(BoxConstraints constraints) => constraints.biggest;

  @override
  BoxConstraints getConstraintsForChild(int i, BoxConstraints constraints) =>
      BoxConstraints.loose(constraints.biggest);

  @override
  void paintChildren(FlowPaintingContext context) {
    final size = context.size;
    final center = Offset(size.width / 2, size.height / 2);
    final radius = math.min(size.width, size.height) / 2;
    final childRadius = radius * radiusFactor;
    final count = context.childCount;

    const double startAngle = math.pi * 0.15;
    const double sweepAngle = math.pi * 0.7;

    for (int i = 0; i < count; i++) {
      final angle = count == 1
          ? startAngle + sweepAngle / 2
          : startAngle + i * (sweepAngle / (count - 1));

      final childSize = context.getChildSize(i) ?? Size.zero;
      final x = center.dx + childRadius * math.cos(angle) - childSize.width / 2;
      final y = center.dy + childRadius * math.sin(angle) - childSize.height / 2;

      context.paintChild(i, transform: Matrix4.translationValues(x, y, 0));
    }
  }

  @override
  bool shouldRepaint(_ArcFlowDelegate old) => radiusFactor != old.radiusFactor;
}
