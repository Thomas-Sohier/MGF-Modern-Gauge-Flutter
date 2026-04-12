import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/routes/route_names.dart';
import 'package:modern_gauge_flutter/ui/screens/multi_metric_screen.dart';

final _metrics = [
  MetricDef(
    label: 'Feedback',
    unit: '%',
    maxValue: 200,
    getValue: (d) => d?.ecuData?.fuellingFeedbackPercent?.toDouble() ?? 0,
  ),
  MetricDef(
    label: 'Trim court',
    unit: '%',
    maxValue: 30,
    getValue: (d) => d?.ecuData?.shortTermTrimPercent?.toDouble() ?? 0,
  ),
  MetricDef(
    label: 'Trim long',
    unit: '%',
    maxValue: 30,
    getValue: (d) => d?.ecuData?.longTermTrim?.toDouble() ?? 0,
  ),
  MetricDef(
    label: 'Inj. 1',
    unit: 'ms',
    maxValue: 15,
    format: (v) => v.toStringAsFixed(2),
    getValue: (d) => d?.ecuData?.injector1Pw?.toDouble() ?? 0,
  ),
  MetricDef(
    label: 'Inj. 2',
    unit: 'ms',
    maxValue: 15,
    format: (v) => v.toStringAsFixed(2),
    getValue: (d) => d?.ecuData?.injector2Pw?.toDouble() ?? 0,
  ),
];

class InjectionScreen extends StatelessWidget {
  const InjectionScreen({super.key});

  @override
  Widget build(BuildContext context) => MultiMetricScreen(
    routeSegment: RouteNames.injectionRoute,
    metrics: _metrics,
  );
}
