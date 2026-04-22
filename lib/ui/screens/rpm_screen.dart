import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/providers/ecu_provider.dart';
import 'package:modern_gauge_flutter/providers/settings_provider.dart';
import 'package:modern_gauge_flutter/routes/navigation_logic.dart';
import 'package:modern_gauge_flutter/routes/route_names.dart';
import 'package:modern_gauge_flutter/ui/screens/multi_metric_screen.dart';
import 'package:modern_gauge_flutter/ui/widgets/dual_arc_dial.dart';
import 'package:modern_gauge_flutter/ui/widgets/gauge_layout.dart';
import 'package:go_router/go_router.dart';
import 'package:provider/provider.dart';

final _metrics = [
  MetricDef.action(
    label: 'ODB',
    icon: (infos) => (infos?.connected ?? false) ? Icons.link : Icons.link_off,
    onTap: (context) {
      context.read<EcuProvider>().retryInitialData();
    },
  ),
  MetricDef(
    label: 'RPM',
    icon: (_) => Icons.speed,
    unit: '',
    maxValue: 8500,
    dangerThreshold: 7000,
    getValue: (d) => d?.ecuData?.rpm?.toDouble() ?? 0,
  ),
  MetricDef(
    label: 'LDR',
    unit: '°C',
    dangerThreshold: 150,
    maxValue: 200,
    icon: (_) => Icons.thermostat,
    getValue: (d) => d?.ecuData?.coolantTemp?.toDouble() ?? 0,
  ),
  MetricDef(
    label: 'Batterie',
    unit: 'V',
    maxValue: 20,
    dangerThreshold: 16,
    icon: (_) => Icons.electric_car,
    format: (v) => v.toStringAsFixed(2),
    getValue: (d) => d?.ecuData?.batteryVoltage?.toDouble() ?? 0,
  ),
  MetricDef(
    label: 'Huile',
    unit: '°C',
    dangerThreshold: 150,
    maxValue: 200,
    icon: (_) => Icons.oil_barrel,
    getValue: (d) => d?.ecuData?.oilTemp?.toDouble() ?? 0,
  ),
];

class RpmScreen extends StatefulWidget {
  const RpmScreen({super.key});

  @override
  State<RpmScreen> createState() => _RpmScreenState();
}

class _RpmScreenState extends State<RpmScreen> {
  int _primaryIndex = 1; // RPM par défaut (index 1, car 0 est ODB action)

  List<int> get _displayableIndices => [
    for (int i = 0; i < _metrics.length; i++)
      if (!_metrics[i].isAction) i,
  ];

  void _cyclePrimary() {
    final indices = _displayableIndices;
    if (indices.length <= 1) return;
    final currentPos = indices.indexOf(_primaryIndex);
    final nextPos = (currentPos + 1) % indices.length;
    setState(() => _primaryIndex = indices[nextPos]);
  }

  void _navigate(bool forward) {
    final enabled = context.read<SettingsProvider>().settings.enabledScreens;
    const route = RouteNames.rpmFull;
    context.go(
      forward ? getNextRoute(route, enabled) : getPreviousRoute(route, enabled),
    );
  }

  @override
  Widget build(BuildContext context) {
    final primary = _metrics[_primaryIndex];

    return GestureDetector(
      onTapUp: (d) {
        final half = MediaQuery.of(context).size.width / 2;
        _navigate(d.globalPosition.dx >= half);
      },
      // deferToChild: outer detector only joins the gesture arena where a child
      // passes hit-testing. InkWell children win the arena (innermost processed
      // first), preventing accidental navigation when tapping interactive widgets.
      behavior: HitTestBehavior.deferToChild,
      child: Stack(
        children: [
          // Combined dual-arc dial — single paint pass for both throttle and primary.
          Positioned.fill(
            child: GaugeLayout(
              backgroundDial: Selector<EcuProvider, (double, double)>(
                selector: (_, ecu) => (
                  ecu.currentData.ecuData?.throttleAngle?.toDouble() ?? 0,
                  primary.getValue(ecu.currentData),
                ),
                builder: (_, values, __) => DualArcDial(
                  throttleValue: values.$1,
                  primaryValue: values.$2,
                  primaryMaxValue: primary.maxValue,
                  primaryDangerThreshold: primary.dangerThreshold,
                ),
              ),
              bottomChildren: _metrics
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
          ),
        ],
      ),
    );
  }
}
