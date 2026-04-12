import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/providers/ecu_provider.dart';
import 'package:modern_gauge_flutter/providers/settings_provider.dart';
import 'package:modern_gauge_flutter/routes/navigation_logic.dart';
import 'package:modern_gauge_flutter/routes/route_names.dart';
import 'package:modern_gauge_flutter/ui/screens/multi_metric_screen.dart';
import 'package:modern_gauge_flutter/ui/widgets/digital_dial.dart';
import 'package:modern_gauge_flutter/ui/widgets/gauge_layout.dart';
import 'package:go_router/go_router.dart';
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

class RpmScreen extends StatefulWidget {
  const RpmScreen({super.key});

  @override
  State<RpmScreen> createState() => _RpmScreenState();
}

class _RpmScreenState extends State<RpmScreen> {
  int _primaryIndex = 1; // RPM par défaut (index 1, car 0 est ODB action)

  List<int> get _displayableIndices => [
    for (int i = 0; i < _metrics.length; i++)
      if (!_metrics[i].isAction) i,
  ];

  void _cyclePrimary() {
    final indices = _displayableIndices;
    if (indices.length <= 1) return;
    final currentPos = indices.indexOf(_primaryIndex);
    final nextPos = (currentPos + 1) % indices.length;
    setState(() => _primaryIndex = indices[nextPos]);
  }

  void _navigate(bool forward) {
    final enabled = context.read<SettingsProvider>().settings.enabledScreens;
    const route = RouteNames.dashboardRoute + RouteNames.rpmRoute;
    context.go(
      forward ? getNextRoute(route, enabled) : getPreviousRoute(route, enabled),
    );
  }

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onTapUp: (d) {
        final half = MediaQuery.of(context).size.width / 2;
        _navigate(d.globalPosition.dx >= half);
      },
      onHorizontalDragEnd: (d) {
        if ((d.primaryVelocity ?? 0).abs() < 100) return;
        _navigate(d.primaryVelocity! < 0);
      },
      behavior: HitTestBehavior.opaque,
      child: Consumer<EcuProvider>(
        builder: (context, ecu, _) {
          final ecuInfos = ecu.currentData;
          final primary = _metrics[_primaryIndex];
          final primaryValue = primary.getValue(ecuInfos);
          final throttle = ecuInfos.ecuData?.throttleAngle?.toDouble() ?? 0;

          return Stack(
            children: [
              // Jauge throttle (extérieure, fine, 1 segment)
              Positioned.fill(
                child: Padding(
                  padding: const EdgeInsets.all(64),
                  child: DigitalDial(
                    value: throttle,
                    maxValue: 100,
                    numberOfSegments: 1,
                    segmentHeight: 12,
                  ),
                ),
              ),
              // Jauge principale + layout
              Positioned.fill(
                child: GaugeLayout(
                  value: primaryValue,
                  maxValue: primary.maxValue,
                  dangerThreshold: primary.dangerThreshold,
                  bottomChildren: _metrics
                      .asMap()
                      .entries
                      .map(
                        (e) => MetricIndicator(
                          metric: e.value,
                          data: ecuInfos,
                          value: e.value.getValue(ecuInfos),
                          isPrimary:
                              !e.value.isAction && e.key == _primaryIndex,
                        ),
                      )
                      .toList(),
                  child: MetricPrimaryDisplay(
                    metric: primary,
                    value: primaryValue,
                    onCycle: _cyclePrimary,
                  ),
                ),
              ),
            ],
          );
        },
      ),
    );
  }
}
