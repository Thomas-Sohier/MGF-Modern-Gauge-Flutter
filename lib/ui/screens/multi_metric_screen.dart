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
import 'package:modern_gauge_flutter/ui/widgets/gauge_layout.dart';
import 'package:provider/provider.dart';

// ── Définition d'une métrique ──────────────────────────────────────────────

class MetricDef {
  final String label;
  final String? unit;
  final double maxValue;
  final double? dangerThreshold;
  final double Function(EcuInfos?) getValue;

  /// Formatage personnalisé de la valeur. Si null, arrondi à l'entier.
  final String Function(double)? format;

  /// Icône optionnelle affichée dans l'indicateur du bas.
  /// Fonction pour permettre une icône dynamique selon les données ECU.
  final IconData Function(EcuInfos?)? icon;

  /// Action optionnelle au tap sur l'indicateur du bas.
  final void Function(BuildContext)? onTap;

  const MetricDef({
    required this.label,
    required this.unit,
    required this.maxValue,
    required this.getValue,
    this.dangerThreshold,
    this.format,
    this.icon,
    this.onTap,
  });

  /// Bouton d'action sans valeur, non sélectionnable comme métrique principale.
  const MetricDef.action({
    required this.label,
    required IconData Function(EcuInfos?) this.icon,
    required this.onTap,
  }) : unit = null,
       maxValue = 0,
       getValue = _zeroValue,
       dangerThreshold = null,
       format = null;

  static double _zeroValue(EcuInfos? _) => 0;

  bool get isAction => maxValue == 0 && getValue == _zeroValue;

  String display(double v) =>
      format != null ? format!(v) : v.round().toString();
}

// ── Écran générique multi-métriques ───────────────────────────────────────

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
  late int _primaryIndex;

  List<int> get _displayableIndices => [
    for (int i = 0; i < widget.metrics.length; i++)
      if (!widget.metrics[i].isAction) i,
  ];

  @override
  void initState() {
    super.initState();
    final indices = _displayableIndices;
    _primaryIndex = indices.isNotEmpty ? indices.first : 0;
  }

  void _cyclePrimary() {
    final indices = _displayableIndices;
    if (indices.length <= 1) return;
    final currentPos = indices.indexOf(_primaryIndex);
    final nextPos = (currentPos + 1) % indices.length;
    setState(() => _primaryIndex = indices[nextPos]);
  }

  void _navigate(bool forward) {
    final enabled = context.read<SettingsProvider>().settings.enabledScreens;
    final route = RouteNames.dashboardRoute + widget.routeSegment;
    context.go(
      forward ? getNextRoute(route, enabled) : getPreviousRoute(route, enabled),
    );
  }

  @override
  Widget build(BuildContext context) {
    final primary = widget.metrics[_primaryIndex];

    return GestureDetector(
      onTapUp: (d) {
        final half = MediaQuery.of(context).size.width / 2;
        _navigate(d.globalPosition.dx >= half);
      },
      // deferToChild: outer detector only joins the gesture arena where a child
      // passes hit-testing. InkWell children win the arena (innermost processed
      // first), preventing accidental navigation when tapping interactive widgets.
      behavior: HitTestBehavior.deferToChild,
      child: GaugeLayout(
        // Selector isolated inside backgroundDial — GaugeLayout itself never
        // rebuilds on value changes, only DigitalDial does.
        backgroundDial: Selector<EcuProvider, double>(
          selector: (_, ecu) => primary.getValue(ecu.currentData),
          builder: (_, primaryValue, __) => DigitalDial(
            value: primaryValue,
            maxValue: primary.maxValue,
            dangerThreshold: primary.dangerThreshold,
          ),
        ),
        bottomChildren: widget.metrics
            .asMap()
            .entries
            .map(
              (e) => MetricIndicator(
                metric: e.value,
                isPrimary: !e.value.isAction && e.key == _primaryIndex,
              ),
            )
            .toList(),
        child: MetricPrimaryDisplay(
          metric: primary,
          onCycle: _cyclePrimary,
        ),
      ),
    );
  }
}

// ── Affichage principal (tappable pour cycler) ─────────────────────────────

/// Subscribes only to its own metric value — no rebuild on unrelated field changes.
class MetricPrimaryDisplay extends StatelessWidget {
  final MetricDef metric;
  final VoidCallback onCycle;

  const MetricPrimaryDisplay({
    super.key,
    required this.metric,
    required this.onCycle,
  });

  @override
  Widget build(BuildContext context) {
    return Selector<EcuProvider, double>(
      selector: (_, ecu) => metric.getValue(ecu.currentData),
      builder: (context, value, _) {
        final gaugeTheme = Theme.of(context).extension<GaugeTheme>()!;
        final isDanger =
            metric.dangerThreshold != null && value >= metric.dangerThreshold!;
        final color =
            isDanger ? gaugeTheme.dangerColor! : gaugeTheme.activeColor!;

        return GestureDetector(
          onTap: onCycle,
          behavior: HitTestBehavior.opaque,
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
                  if (metric.unit != null)
                    Text(metric.unit!, style: AppTextStyles.small),
                ],
              ),
              Text(metric.label, style: AppTextStyles.unit(color)),
            ],
          ),
        );
      },
    );
  }
}

// ── Indicateur bas (toutes les métriques, primaire mis en évidence) ────────

/// Subscribes only to its specific metric value and icon — no rebuild on
/// unrelated field changes.
class MetricIndicator extends StatelessWidget {
  final MetricDef metric;
  final bool isPrimary;

  const MetricIndicator({
    super.key,
    required this.metric,
    required this.isPrimary,
  });

  @override
  Widget build(BuildContext context) {
    return Selector<EcuProvider, (double, IconData?)>(
      selector: (_, ecu) => (
        metric.getValue(ecu.currentData),
        metric.icon?.call(ecu.currentData),
      ),
      builder: (context, selection, _) {
        final (value, iconData) = selection;
        final cs = Theme.of(context).colorScheme;
        final gaugeTheme = Theme.of(context).extension<GaugeTheme>()!;

        final valueColor = isPrimary
            ? gaugeTheme.activeColor!
            : cs.onSurface.withValues(alpha: 0.85);

        final content = SizedBox(
          width: 90,
          height: 90,
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            crossAxisAlignment: CrossAxisAlignment.center,
            children: [
              if (iconData != null) ...[
                Icon(iconData, size: 28, color: valueColor),
                const SizedBox(height: 2),
              ],
              if (!metric.isAction)
                Row(
                  crossAxisAlignment: CrossAxisAlignment.center,
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    Text(
                      metric.display(value),
                      style: AppTextStyles.label.copyWith(color: valueColor),
                    ),
                    if (metric.unit != null)
                      Text(
                        metric.unit!,
                        maxLines: 1,
                        style: AppTextStyles.label.copyWith(color: valueColor),
                      ),
                  ],
                ),
              if (iconData == null && !metric.isAction) const SizedBox(height: 2),
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

        if (metric.onTap != null) {
          return GestureDetector(
            onTap: () => metric.onTap!(context),
            behavior: HitTestBehavior.opaque,
            child: content,
          );
        }
        return content;
      },
    );
  }
}
