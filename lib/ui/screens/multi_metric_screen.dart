import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';
import 'package:modern_gauge_flutter/models/ecu_data.dart';
import 'package:modern_gauge_flutter/providers/ecu_provider.dart';
import 'package:modern_gauge_flutter/providers/settings_provider.dart';
import 'package:modern_gauge_flutter/routes/navigation_logic.dart';
import 'package:modern_gauge_flutter/routes/route_names.dart';
import 'package:modern_gauge_flutter/ui/themes/app_text_styles.dart';
import 'package:modern_gauge_flutter/ui/themes/gauge_theme.dart';
import 'package:modern_gauge_flutter/ui/widgets/digital_dial.dart';
import 'package:provider/provider.dart';

// ── Définition d'une métrique ──────────────────────────────────────────────

class MetricDef {
  final String label;
  final String unit;
  final double maxValue;
  final double? dangerThreshold;
  final double Function(EcuData?) getValue;

  /// Formatage personnalisé de la valeur. Si null, arrondi à l'entier.
  final String Function(double)? format;

  const MetricDef({
    required this.label,
    required this.unit,
    required this.maxValue,
    required this.getValue,
    this.dangerThreshold,
    this.format,
  });

  String display(double v) =>
      format != null ? format!(v) : v.round().toString();
}

// ── Écran générique multi-métriques ───────────────────────────────────────

/// Écran avec une métrique principale affichée dans le DigitalDial.
/// • Tap sur la valeur centrale  → cycle vers la métrique suivante.
/// • Tap gauche / droite (hors centre) → écran précédent / suivant.
/// • Swipe horizontal → navigation écrans.
/// Toutes les métriques sont aussi visibles en bas, la principale est mise en évidence.
class MultiMetricScreen extends StatefulWidget {
  final String routeSegment;
  final List<MetricDef> metrics;

  const MultiMetricScreen({
    super.key,
    required this.routeSegment,
    required this.metrics,
  });

  @override
  State<MultiMetricScreen> createState() => _MultiMetricScreenState();
}

class _MultiMetricScreenState extends State<MultiMetricScreen> {
  int _primaryIndex = 0;

  void _cyclePrimary() => setState(
    () => _primaryIndex = (_primaryIndex + 1) % widget.metrics.length,
  );

  void _navigate(bool forward) {
    final enabled = context.read<SettingsProvider>().settings.enabledScreens;
    final route = RouteNames.dashboardRoute + widget.routeSegment;
    context.go(
      forward ? getNextRoute(route, enabled) : getPreviousRoute(route, enabled),
    );
  }

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      // Tap gauche < 50% → précédent, droit > 50% → suivant.
      // Le GestureDetector interne (_PrimaryDisplay) gagne l'arène pour les taps
      // sur la zone centrale, donc onTapUp ne se déclenche pas sur le centre.
      onTapUp: (d) {
        final half = MediaQuery.of(context).size.width / 2;
        _navigate(d.globalPosition.dx >= half);
      },
      onHorizontalDragEnd: (d) {
        if ((d.primaryVelocity ?? 0).abs() < 100) return;
        _navigate(d.primaryVelocity! < 0);
      },
      behavior: HitTestBehavior.opaque,
      child: Consumer<EcuProvider>(
        builder: (context, ecu, _) {
          final data = ecu.currentData.ecuData;
          final primary = widget.metrics[_primaryIndex];
          final primaryValue = primary.getValue(data);

          return DigitalDial(
            value: primaryValue,
            maxValue: primary.maxValue,
            unit: primary.unit,
            dangerThreshold: primary.dangerThreshold,
            showGaugeBorder: false,
            gaugeBorderSpacing: 0,
            gaugeBorderWidth: 0,
            segmentHeight: 40,
            numberOfSegments: 20,
            bottomChildrenRadiusFactor: 0.75,
            bottomChildren: widget.metrics
                .asMap()
                .entries
                .map(
                  (e) => _BottomIndicator(
                    metric: e.value,
                    value: e.value.getValue(data),
                    isPrimary: e.key == _primaryIndex,
                  ),
                )
                .toList(),
            child: _PrimaryDisplay(
              metric: primary,
              value: primaryValue,
              onCycle: _cyclePrimary,
            ),
          );
        },
      ),
    );
  }
}

// ── Affichage principal (tappable pour cycler) ─────────────────────────────

class _PrimaryDisplay extends StatelessWidget {
  final MetricDef metric;
  final double value;
  final VoidCallback onCycle;

  const _PrimaryDisplay({
    required this.metric,
    required this.value,
    required this.onCycle,
  });

  @override
  Widget build(BuildContext context) {
    final gaugeTheme = Theme.of(context).extension<GaugeTheme>()!;
    final isDanger =
        metric.dangerThreshold != null && value >= metric.dangerThreshold!;
    final color = isDanger ? gaugeTheme.dangerColor! : gaugeTheme.activeColor!;

    return Material(
      color: Colors.transparent,
      child: InkWell(
        onTap: onCycle,
        borderRadius: BorderRadius.circular(100),
        child: Column(
          mainAxisSize: MainAxisSize.max,
          mainAxisAlignment: MainAxisAlignment.center,
          crossAxisAlignment: CrossAxisAlignment.center,
          children: [
            Row(
              crossAxisAlignment: CrossAxisAlignment.center,
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                Text(
                  metric.display(value),
                  style: AppTextStyles.display(color),
                ),
                const SizedBox(height: 4),
                Text(metric.unit, style: AppTextStyles.small),
              ],
            ),
            Row(
              crossAxisAlignment: CrossAxisAlignment.center,
              mainAxisAlignment: MainAxisAlignment.center,
              mainAxisSize: MainAxisSize.min,
              children: [
                Text(metric.label, style: AppTextStyles.unit(color)),
                const SizedBox(width: 6),
                Icon(
                  Icons.sync_rounded,
                  size: 14,
                  color: color.withValues(alpha: 0.45),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}

// ── Indicateur bas (toutes les métriques, primaire mis en évidence) ────────

class _BottomIndicator extends StatelessWidget {
  final MetricDef metric;
  final double value;
  final bool isPrimary;

  const _BottomIndicator({
    required this.metric,
    required this.value,
    required this.isPrimary,
  });

  @override
  Widget build(BuildContext context) {
    final cs = Theme.of(context).colorScheme;
    final gaugeTheme = Theme.of(context).extension<GaugeTheme>()!;

    final valueColor = isPrimary
        ? gaugeTheme.activeColor!
        : cs.onSurface.withValues(alpha: 0.85);

    return SizedBox(
      width: 90,
      height: 90,
      child: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          Row(
            crossAxisAlignment: CrossAxisAlignment.center,
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              Text(
                metric.display(value),
                style: AppTextStyles.label.copyWith(color: valueColor),
              ),
              Text(
                metric.unit,
                maxLines: 1,
                style: AppTextStyles.label.copyWith(color: valueColor),
              ),
            ],
          ),
          const SizedBox(height: 2),
          Text(
            metric.label,
            maxLines: 1,
            overflow: TextOverflow.ellipsis,
            style: AppTextStyles.label.copyWith(
              color: valueColor,
              fontSize: 12,
            ),
          ),
        ],
      ),
    );
  }
}
