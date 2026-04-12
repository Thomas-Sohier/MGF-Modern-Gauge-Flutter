import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/providers/ecu_provider.dart';
import 'package:modern_gauge_flutter/routes/route_names.dart';
import 'package:modern_gauge_flutter/ui/screens/multi_metric_screen.dart';
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

class RpmScreen extends StatelessWidget {
  const RpmScreen({super.key});

  @override
  Widget build(BuildContext context) =>
      MultiMetricScreen(routeSegment: RouteNames.rpmRoute, metrics: _metrics);
}
