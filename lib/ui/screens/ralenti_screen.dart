import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/routes/route_names.dart';
import 'package:modern_gauge_flutter/ui/screens/multi_metric_screen.dart';

final _metrics = [
  MetricDef(
    label: 'Consigne',
    unit: 'rpm',
    maxValue: 2000,
    getValue: (d) => d?.data.idleSetpointValue ?? 0,
  ),
  MetricDef(
    label: 'Vanne',
    unit: '%',
    maxValue: 100,
    getValue: (d) => d?.data.idleValvePositionValue ?? 0,
  ),
  MetricDef(
    label: 'Base',
    unit: '%',
    maxValue: 100,
    getValue: (d) => d?.data.idleBasePositionValue ?? 0,
  ),
  MetricDef(
    label: 'Erreur',
    unit: 'rpm',
    maxValue: 300,
    getValue: (d) => d?.data.idleErrorValue ?? 0,
  ),
  MetricDef(
    label: 'Ajusteur',
    unit: 'rpm',
    maxValue: 300,
    getValue: (d) => d?.data.idleAdjusterRpmValue ?? 0,
  ),
];

class RalentiScreen extends StatelessWidget {
  const RalentiScreen({super.key});

  @override
  Widget build(BuildContext context) => MultiMetricScreen(
    routeSegment: RouteNames.ralentiRoute,
    metrics: _metrics,
  );
}
