import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/routes/route_names.dart';
import 'package:modern_gauge_flutter/ui/screens/multi_metric_screen.dart';

final _metrics = [
  MetricDef(
    label: 'Lambda',
    unit: 'mV',
    maxValue: 1000,
    icon: (_) => Icons.sensors_rounded,
    getValue: (d) => d?.ecuData?.lambdaMv?.toDouble() ?? 0,
  ),
  MetricDef(
    label: 'O2',
    unit: 'mV',
    maxValue: 1000,
    icon: (_) => Icons.air_rounded,
    getValue: (d) => d?.ecuData?.o2Mv?.toDouble() ?? 0,
  ),
  MetricDef(
    label: 'AFR',
    unit: '',
    maxValue: 20,
    dangerThreshold: 17,
    icon: (_) => Icons.local_gas_station_rounded,
    format: (v) => v.toStringAsFixed(2),
    getValue: (d) => d?.ecuData?.estimateAirFuel?.toDouble() ?? 0,
  ),
  MetricDef(
    label: 'Duty cycle',
    unit: '%',
    maxValue: 100,
    icon: (_) => Icons.speed_rounded,
    getValue: (d) => d?.ecuData?.lambdaSensorDutyCycle?.toDouble() ?? 0,
  ),
];

class LambdaScreen extends StatelessWidget {
  const LambdaScreen({super.key});

  @override
  Widget build(BuildContext context) => MultiMetricScreen(
    routeSegment: RouteNames.lambdaRoute,
    metrics: _metrics,
  );
}
