import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/routes/route_names.dart';
import 'package:modern_gauge_flutter/ui/screens/multi_metric_screen.dart';

final _metrics = [
  MetricDef(
    label: 'Eau',
    unit: '°C',
    maxValue: 150,
    dangerThreshold: 105,
    getValue: (d) => d?.data.coolantTempValue ?? 0,
  ),
  MetricDef(
    label: 'Huile',
    unit: '°C',
    maxValue: 160,
    dangerThreshold: 130,
    getValue: (d) => d?.data.oilTempValue ?? 0,
  ),
  MetricDef(
    label: 'Admission',
    unit: '°C',
    maxValue: 80,
    getValue: (d) => d?.data.intakeAirTempValue ?? 0,
  ),
  MetricDef(
    label: 'Ambiante',
    unit: '°C',
    maxValue: 60,
    getValue: (d) => d?.data.ambientTempValue ?? 0,
  ),
  MetricDef(
    label: 'Carburant',
    unit: '°C',
    maxValue: 120,
    dangerThreshold: 90,
    getValue: (d) => d?.data.fuelRailTempValue ?? 0,
  ),
];

class TempsScreen extends StatelessWidget {
  const TempsScreen({super.key});

  @override
  Widget build(BuildContext context) =>
      MultiMetricScreen(routeSegment: RouteNames.tempsRoute, metrics: _metrics);
}
