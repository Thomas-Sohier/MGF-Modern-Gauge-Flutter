import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/routes/route_names.dart';
import 'package:modern_gauge_flutter/ui/screens/multi_metric_screen.dart';

final _metrics = [
  MetricDef(
    label: 'Lambda',
    unit: 'mV',
    maxValue: 1000,
    getValue: (d) => d?.lambdaMv?.toDouble() ?? 0,
  ),
  MetricDef(
    label: 'O2',
    unit: 'mV',
    maxValue: 1000,
    getValue: (d) => d?.o2Mv?.toDouble() ?? 0,
  ),
  MetricDef(
    label: 'AFR',
    unit: '',
    maxValue: 20,
    dangerThreshold: 17,
    format: (v) => v.toStringAsFixed(2),
    getValue: (d) => d?.estimateAirFuel?.toDouble() ?? 0,
  ),
  MetricDef(
    label: 'Duty cycle',
    unit: '%',
    maxValue: 100,
    getValue: (d) => d?.lambdaSensorDutyCycle?.toDouble() ?? 0,
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
