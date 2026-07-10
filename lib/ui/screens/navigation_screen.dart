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
    // Design 1a : icône + distance empilées en haut, bandeau vert en corde.
    // Content is clipped to the round display centrally (CircularContent), so
    // the full-width green ETA banner follows the circle's bottom chord
    // (Google Maps footer adapted to a round screen).
    return Column(
      children: [
        Expanded(
          child: Column(
            mainAxisSize: MainAxisSize.max,
            crossAxisAlignment: CrossAxisAlignment.center,
            mainAxisAlignment: MainAxisAlignment.end,
            children: [
              _ManeuverIcon(iconUrl: nav.iconUrl, color: theme.primaryColor),
              const SizedBox(height: 10),
              if (nav.instruction != null)
                _Instruction(
                  text: nav.instruction!,
                  color: theme.colorScheme.onSurface,
                ),
            ],
          ),
        ),
        if (hasFooter) _EtaBanner(distance: nav.distance, eta: nav.eta),
      ],
    );
  }
}

/// Instruction principale : quand elle commence par un chiffre (ex. "300 m"),
/// le nombre est affiché en très grand avec l'unité en plus petit sur la même
/// ligne de base ; sinon le texte est affiché tel quel, centré.
class _Instruction extends StatelessWidget {
  static final RegExp _numeric = RegExp(r'^([\d.,]+)\s*(\D*)$');

  final String text;
  final Color color;

  const _Instruction({required this.text, required this.color});

  @override
  Widget build(BuildContext context) {
    final match = _numeric.firstMatch(text);
    if (match == null) {
      return Padding(
        padding: const EdgeInsets.symmetric(horizontal: 36),
        child: Text(
          text,
          textAlign: TextAlign.center,
          maxLines: 3,
          overflow: TextOverflow.ellipsis,
          style: AppTextStyles.body.copyWith(fontSize: 26, color: color),
        ),
      );
    }
    final unit = match.group(2)!;
    return Row(
      mainAxisSize: MainAxisSize.min,
      crossAxisAlignment: CrossAxisAlignment.baseline,
      textBaseline: TextBaseline.alphabetic,
      children: [
        Text(
          match.group(1)!,
          style: AppTextStyles.display(color).copyWith(
            fontSize: 110,
            fontWeight: FontWeight.w900,
            letterSpacing: -3,
          ),
        ),
        if (unit.isNotEmpty) ...[
          const SizedBox(width: 8),
          Text(unit, style: AppTextStyles.unit(color).copyWith(fontSize: 30)),
        ],
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
    final fallback = Icon(Icons.navigation_rounded, size: 88, color: color);
    if (iconUrl == null) return fallback;
    // The maneuver icon is a monochrome alpha PNG; tint it to the theme color.
    return Image.network(
      iconUrl!,
      width: 88,
      height: 88,
      color: color,
      gaplessPlayback: true,
      filterQuality: FilterQuality.medium,
      errorBuilder: (_, __, ___) => fallback,
    );
  }
}

/// Google-Maps-style green footer banner (design 1a). Spans the full width
/// and is clipped to the circle's bottom chord by the parent [ClipOval]; the
/// extra bottom padding keeps the text clear of the curved edge. The street
/// ([distance]) is the prominent line, the [eta] the secondary one.
class _EtaBanner extends StatelessWidget {
  /// Google Maps navigation footer green.
  static const Color _green = Color(0xFF1E8E3E);

  final String? distance;
  final String? eta;

  const _EtaBanner({required this.distance, required this.eta});

  @override
  Widget build(BuildContext context) {
    return Container(
      width: double.infinity,
      constraints: const BoxConstraints(minHeight: 156),
      color: _green,
      padding: const EdgeInsets.fromLTRB(40, 15, 40, 30),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          if (distance != null)
            Text(
              distance!,
              textAlign: TextAlign.center,
              maxLines: 2,
              overflow: TextOverflow.ellipsis,
              style: AppTextStyles.title.copyWith(
                fontSize: 30,
                color: Colors.white,
              ),
            ),
          if (eta != null) ...[
            const SizedBox(height: 16),
            ConstrainedBox(
              constraints: const BoxConstraints(maxWidth: 200),
              child: Text(
                eta!,
                textAlign: TextAlign.center,
                maxLines: 2,
                overflow: TextOverflow.ellipsis,
                style: AppTextStyles.small.copyWith(
                  fontSize: 18,
                  color: Colors.white.withValues(alpha: 0.85),
                ),
              ),
            ),
          ],
        ],
      ),
    );
  }
}
