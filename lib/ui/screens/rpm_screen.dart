import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/providers/ecu_provider.dart';
import 'package:modern_gauge_flutter/routes/route_names.dart';
import 'package:modern_gauge_flutter/ui/screens/multi_metric_screen.dart';
import 'package:provider/provider.dart';

final _metrics = [
  MetricDef(
    label: 'RPM',
    icon: Icons.speed,
    maxValue: 8500,
    dangerThreshold: 7000,
    getValue: (d) => d?.rpm?.toDouble() ?? 0,
  ),
  MetricDef(
    label: 'ODB',
    icon: Icons.link,
    onTap: (context) {
      context.read<EcuProvider>().retryInitialData();
    },
    maxValue: 0,
    getValue: (d) => 0,
  ),
  MetricDef(
    label: 'LDR',
    unit: '°C',
    dangerThreshold: 150,
    maxValue: 200,
    icon: Icons.thermostat,
    getValue: (d) => d?.coolantTemp?.toDouble() ?? 0,
  ),
  MetricDef(
    label: 'Batterie',
    unit: 'V',
    maxValue: 20,
    dangerThreshold: 16,
    icon: Icons.electric_car,
    format: (v) => v.toStringAsFixed(2),
    getValue: (d) => d?.batteryVoltage?.toDouble() ?? 0,
  ),
  MetricDef(
    label: 'Huile',
    unit: '°C',
    dangerThreshold: 150,
    maxValue: 200,
    icon: Icons.oil_barrel,
    getValue: (d) => d?.oilTemp?.toDouble() ?? 0,
  ),
];

class RpmScreen extends StatelessWidget {
  const RpmScreen({super.key});

  @override
  Widget build(BuildContext context) =>
      MultiMetricScreen(routeSegment: RouteNames.rpmRoute, metrics: _metrics);
}
