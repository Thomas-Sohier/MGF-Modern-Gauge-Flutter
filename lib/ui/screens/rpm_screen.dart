import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';
import 'package:modern_gauge_flutter/mixins/screen_navigation_mixin.dart';
import 'package:modern_gauge_flutter/providers/app_state_provider.dart';
import 'package:modern_gauge_flutter/providers/dial_provider.dart';
import 'package:modern_gauge_flutter/providers/ecu_provider.dart';
import 'package:modern_gauge_flutter/routes/navigation_logic.dart';
import 'package:modern_gauge_flutter/routes/route_names.dart';
import 'package:modern_gauge_flutter/services/odb_service.dart';
import 'package:modern_gauge_flutter/ui/widgets/digital_dial.dart';
import 'package:provider/provider.dart';

/// The main dashboard screen displaying all gauges.
class RpmScreen extends StatefulWidget {
  const RpmScreen({super.key});

  @override
  State<RpmScreen> createState() => _RpmScreenState();
}

class _RpmScreenState extends State<RpmScreen>
    with ScreenNavigationMixin<RpmScreen> {
  late final OdbService _odbService;

  @override
  void nextScreen() {
    const currentRoute = RouteNames.dashboardRoute + RouteNames.rpmRoute;
    context.go(getNextRoute(currentRoute));
  }

  @override
  void previousScreen() {
    const currentRoute = RouteNames.dashboardRoute + RouteNames.rpmRoute;
    context.go(getPreviousRoute(currentRoute));
  }

  @override
  void initState() {
    super.initState();
    _odbService = context.read<OdbService>();
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (mounted) {
        _odbService.startOdbDataStream();
      }
    });
  }

  @override
  void dispose() {
    _odbService.stopOdbDataStream();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return buildNavigableScreen(child: _Dial());
  }
}

/// Un widget interne qui écoute les changements et reconstruit UNIQUEMENT la jauge.
class _Dial extends StatelessWidget {
  const _Dial();

  @override
  Widget build(BuildContext context) {
    return Selector<DialProvider, double>(
      selector: (_, service) => service.dialData.rpm,
      builder: (_, rpmValue, __) {
        return DigitalDial(
          value: rpmValue,
          maxValue: 7000,
          unit: 'RPM',
          dangerThreshold: 6000,
          showGaugeBorder: false,
          gaugeBorderSpacing: 0,
          gaugeBorderWidth: 0,
          segmentHeight: 40,
          numberOfSegments: 20,
          bottomChildrenRadiusFactor: 0.75,
          bottomChildren: const [
            _OdbIndicator(),
            _CoolantTempIndicator(),
            _OilTempIndicator(),
            _BatteryIndicator(),
          ],
        );
      },
    );
  }
}

/// Indicateur qui écoute SEULEMENT le statut ODB de AppStateProvider.
class _OdbIndicator extends StatelessWidget {
  const _OdbIndicator();

  @override
  Widget build(BuildContext context) {
    return InkWell(
      onTap: () {
        context.read<EcuProvider>().retryInitialData();
      },
      child: Consumer<AppStateProvider>(
        builder: (context, appState, _) {
          final isConnected =
              appState.odbStatus == OdbConnectionStatus.connected;
          return _IndicatorBase(
            icon: isConnected ? Icons.link : Icons.link_off,
            label: 'ODB',
          );
        },
      ),
    );
  }
}

/// Indicateur qui écoute SEULEMENT la température du liquide de refroidissement.
class _CoolantTempIndicator extends StatelessWidget {
  const _CoolantTempIndicator();

  @override
  Widget build(BuildContext context) {
    return Selector<DialProvider, double>(
      selector: (_, dialProvider) => dialProvider.dialData.coolantTemp,
      builder: (context, temp, _) {
        return _IndicatorBase(
          icon: Icons.thermostat,
          label: '${temp.round()}°c',
        );
      },
    );
  }
}

/// Indicateur qui écoute SEULEMENT le voltage de la batterie.
class _BatteryIndicator extends StatelessWidget {
  const _BatteryIndicator();

  @override
  Widget build(BuildContext context) {
    return Selector<DialProvider, double>(
      selector: (_, dialProvider) => dialProvider.dialData.batteryVoltage,
      builder: (context, voltage, _) {
        return _IndicatorBase(
          icon: Icons.electric_car,
          label: '${voltage.toStringAsFixed(1)}V',
        );
      },
    );
  }
}

/// Indicateur qui écoute SEULEMENT la température d'huile.
class _OilTempIndicator extends StatelessWidget {
  const _OilTempIndicator();

  @override
  Widget build(BuildContext context) {
    return Selector<DialProvider, double>(
      selector: (_, dialProvider) => dialProvider.dialData.oilTemp,
      builder: (context, oilTemp, _) {
        return _IndicatorBase(
          icon: Icons.water_drop,
          label: '${oilTemp.toStringAsFixed(1)}°c',
        );
      },
    );
  }
}

/// Un widget de base pour l'apparence des indicateurs, afin d'éviter la répétition de code.
class _IndicatorBase extends StatelessWidget {
  final IconData icon;
  final String label;

  const _IndicatorBase({required this.icon, required this.label});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      width: 90,
      height: 90,
      child: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        crossAxisAlignment: CrossAxisAlignment.center,
        children: [
          Icon(icon, size: 28),
          const SizedBox(height: 2),
          Text(
            label,
            maxLines: 1,
            style: Theme.of(context).textTheme.labelMedium?.copyWith(
              fontWeight: FontWeight.w700,
              fontSize: 18,
              fontFamily: 'JetBrainsMono',
            ),
          ),
        ],
      ),
    );
  }
}
