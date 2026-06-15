import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';
import 'package:modern_gauge_flutter/mixins/screen_navigation_mixin.dart';
import 'package:modern_gauge_flutter/models/navigation_info.dart';
import 'package:modern_gauge_flutter/routes/navigation_logic.dart';
import 'package:modern_gauge_flutter/routes/route_names.dart';
import 'package:modern_gauge_flutter/services/navigation_server_listener.dart';
import 'package:modern_gauge_flutter/ui/themes/app_text_styles.dart';
import 'package:provider/provider.dart';

/// Dashboard screen showing the current turn-by-turn navigation forwarded from
/// the phone (instruction, distance, ETA and the maneuver icon). Shows a
/// placeholder when no navigation is active.
class NavigationScreen extends StatefulWidget {
  const NavigationScreen({super.key});

  @override
  State<NavigationScreen> createState() => _NavigationScreenState();
}

class _NavigationScreenState extends State<NavigationScreen>
    with ScreenNavigationMixin<NavigationScreen> {
  @override
  void nextScreen() =>
      context.go(getNextRoute(RouteNames.navigationFull, enabledScreens));

  @override
  void previousScreen() =>
      context.go(getPreviousRoute(RouteNames.navigationFull, enabledScreens));

  @override
  Widget build(BuildContext context) {
    return buildNavigableScreen(
      child: Consumer<NavigationServerListener>(
        builder: (context, listener, _) {
          final nav = listener.navigation;
          return nav.active ? _NavigationUI(nav: nav) : const _NoNavigationUI();
        },
      ),
    );
  }
}

class _NoNavigationUI extends StatelessWidget {
  const _NoNavigationUI();

  @override
  Widget build(BuildContext context) {
    return const Column(
      mainAxisAlignment: MainAxisAlignment.center,
      children: [
        Icon(Icons.navigation_outlined, size: 100),
        SizedBox(height: 30),
        Text('Aucune navigation en cours...', textAlign: TextAlign.center),
      ],
    );
  }
}

class _NavigationUI extends StatelessWidget {
  final NavigationInfo nav;

  const _NavigationUI({required this.nav});

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Center(
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 40),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            _ManeuverIcon(iconUrl: nav.iconUrl, color: theme.primaryColor),
            const SizedBox(height: 20),
            if (nav.instruction != null)
              Text(
                nav.instruction!,
                textAlign: TextAlign.center,
                maxLines: 3,
                overflow: TextOverflow.ellipsis,
                style: AppTextStyles.body.copyWith(fontWeight: FontWeight.bold),
              ),
            const SizedBox(height: 14),
            _DistanceEta(distance: nav.distance, eta: nav.eta),
          ],
        ),
      ),
    );
  }
}

class _ManeuverIcon extends StatelessWidget {
  final String? iconUrl;
  final Color color;

  const _ManeuverIcon({required this.iconUrl, required this.color});

  @override
  Widget build(BuildContext context) {
    final fallback = Icon(Icons.navigation_rounded, size: 96, color: color);
    if (iconUrl == null) return fallback;
    // The maneuver icon is a monochrome alpha PNG; tint it to the theme color.
    return Image.network(
      iconUrl!,
      width: 96,
      height: 96,
      color: color,
      gaplessPlayback: true,
      filterQuality: FilterQuality.medium,
      errorBuilder: (_, __, ___) => fallback,
    );
  }
}

class _DistanceEta extends StatelessWidget {
  final String? distance;
  final String? eta;

  const _DistanceEta({required this.distance, required this.eta});

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final style = AppTextStyles.small.copyWith(color: theme.colorScheme.onSurface);
    final parts = [
      if (distance != null) distance!,
      if (eta != null) eta!,
    ];
    if (parts.isEmpty) return const SizedBox.shrink();
    return Text(parts.join('  ·  '), textAlign: TextAlign.center, style: style);
  }
}
