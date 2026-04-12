import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/routes/route_names.dart';
import 'package:modern_gauge_flutter/ui/screens/multi_metric_screen.dart';

final _metrics = [
  MetricDef(
    label: 'MAP',
    unit: 'kPa',
    maxValue: 200,
    getValue: (d) => d?.mapSensorKpa?.toDouble() ?? 0,
  ),
  MetricDef(
    label: 'Papillon',
    unit: '°',
    maxValue: 100,
    getValue: (d) => d?.throttleAngle?.toDouble() ?? 0,
  ),
  MetricDef(
    label: 'TPS',
    unit: 'V',
    maxValue: 5,
    format: (v) => v.toStringAsFixed(2),
    getValue: (d) => d?.throttlePotVoltage?.toDouble() ?? 0,
  ),
  MetricDef(
    label: 'Air admis.',
    unit: '°C',
    maxValue: 80,
    getValue: (d) => d?.intakeAirTemp?.toDouble() ?? 0,
  ),
];

class AdmissionScreen extends StatelessWidget {
  const AdmissionScreen({super.key});

  @override
  Widget build(BuildContext context) => MultiMetricScreen(
        routeSegment: RouteNames.admissionRoute,
        metrics: _metrics,
      );
}
