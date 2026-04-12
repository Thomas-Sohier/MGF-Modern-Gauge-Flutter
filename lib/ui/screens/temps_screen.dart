import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/routes/route_names.dart';
import 'package:modern_gauge_flutter/ui/screens/multi_metric_screen.dart';

final _metrics = [
  MetricDef(
    label: 'Eau',
    unit: '°C',
    maxValue: 150,
    dangerThreshold: 105,
    getValue: (d) => d?.coolantTemp?.toDouble() ?? 0,
  ),
  MetricDef(
    label: 'Huile',
    unit: '°C',
    maxValue: 160,
    dangerThreshold: 130,
    getValue: (d) => d?.oilTemp?.toDouble() ?? 0,
  ),
  MetricDef(
    label: 'Admission',
    unit: '°C',
    maxValue: 80,
    getValue: (d) => d?.intakeAirTemp?.toDouble() ?? 0,
  ),
  MetricDef(
    label: 'Ambiante',
    unit: '°C',
    maxValue: 60,
    getValue: (d) => d?.ambientTemp?.toDouble() ?? 0,
  ),
  MetricDef(
    label: 'Carburant',
    unit: '°C',
    maxValue: 120,
    dangerThreshold: 90,
    getValue: (d) => d?.fuelRailTemp?.toDouble() ?? 0,
  ),
];

class TempsScreen extends StatelessWidget {
  const TempsScreen({super.key});

  @override
  Widget build(BuildContext context) => MultiMetricScreen(
        routeSegment: RouteNames.tempsRoute,
        metrics: _metrics,
      );
}
