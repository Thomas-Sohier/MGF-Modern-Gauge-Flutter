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
    final hasFooter = nav.distance != null || nav.eta != null;
    // Content is clipped to the round display centrally (CircularContent), so
    // the full-width green ETA banner follows the circle's bottom chord
    // (Google Maps footer adapted to a round screen).
    return Stack(
      fit: StackFit.expand,
      children: [
        Align(
          alignment: const Alignment(0, -0.35),
          child: Padding(
            padding: const EdgeInsets.symmetric(horizontal: 50),
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                _ManeuverIcon(
                  iconUrl: nav.iconUrl,
                  color: theme.primaryColor,
                ),
                const SizedBox(height: 20),
                if (nav.instruction != null)
                  Text(
                    nav.instruction!,
                    textAlign: TextAlign.center,
                    maxLines: 3,
                    overflow: TextOverflow.ellipsis,
                    style: AppTextStyles.body.copyWith(
                      fontWeight: FontWeight.bold,
                    ),
                  ),
              ],
            ),
          ),
        ),
        if (hasFooter)
          Align(
            alignment: Alignment.bottomCenter,
            child: _EtaBanner(distance: nav.distance, eta: nav.eta),
          ),
      ],
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

/// Google-Maps-style green footer banner. Spans the full width and is clipped
/// to the circle's bottom chord by the parent [ClipOval]; the extra bottom
/// padding keeps the text clear of the curved edge.
class _EtaBanner extends StatelessWidget {
  /// Google Maps navigation footer green.
  static const Color _green = Color(0xFF1A8E3C);

  final String? distance;
  final String? eta;

  const _EtaBanner({required this.distance, required this.eta});

  @override
  Widget build(BuildContext context) {
    return Container(
      width: double.infinity,
      color: _green,
      padding: const EdgeInsets.only(top: 14, bottom: 34),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          if (eta != null)
            Text(
              eta!,
              textAlign: TextAlign.center,
              style: AppTextStyles.title.copyWith(color: Colors.white),
            ),
          if (distance != null)
            Text(
              distance!,
              textAlign: TextAlign.center,
              style: AppTextStyles.small.copyWith(color: Colors.white),
            ),
        ],
      ),
    );
  }
}
