import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/routes/route_names.dart';
import 'package:modern_gauge_flutter/ui/screens/multi_metric_screen.dart';

final _metrics = [
  MetricDef(
    label: 'Avance',
    unit: '°',
    maxValue: 50,
    getValue: (d) => d?.ignitionAdvance?.toDouble() ?? 0,
  ),
  MetricDef(
    label: 'Offset avance',
    unit: '°',
    maxValue: 20,
    getValue: (d) => d?.ignitionAdvanceOffset?.toDouble() ?? 0,
  ),
  MetricDef(
    label: 'Bobine 1',
    unit: 'ms',
    maxValue: 5,
    format: (v) => v.toStringAsFixed(2),
    getValue: (d) => d?.coil1ChargeTime?.toDouble() ?? 0,
  ),
  MetricDef(
    label: 'Bobine 2',
    unit: 'ms',
    maxValue: 5,
    format: (v) => v.toStringAsFixed(2),
    getValue: (d) => d?.coil2ChargeTime?.toDouble() ?? 0,
  ),
  MetricDef(
    label: 'Durée bobine',
    unit: 'µs',
    maxValue: 5000,
    getValue: (d) => d?.coilTimeMicroseconds?.toDouble() ?? 0,
  ),
];

class AllumageScreen extends StatelessWidget {
  const AllumageScreen({super.key});

  @override
  Widget build(BuildContext context) => MultiMetricScreen(
        routeSegment: RouteNames.allumageRoute,
        metrics: _metrics,
      );
}
