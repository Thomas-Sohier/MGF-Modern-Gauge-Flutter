// /ui/screens/dashboard_shell_screen.dart

import 'package:flutter/material.dart';
import 'package:flutter/services.dart'; // Pour RawKeyboardListener et LogicalKeyboardKey
import 'package:go_router/go_router.dart';
import 'package:modern_gauge_flutter/routes/navigation_logic.dart';
import 'package:modern_gauge_flutter/routes/route_names.dart';
import 'package:modern_gauge_flutter/ui/widgets/gauge_background.dart';

class DashboardShellScreen extends StatefulWidget {
  final Widget child;

  const DashboardShellScreen({super.key, required this.child});

  @override
  State<DashboardShellScreen> createState() => _DashboardShellScreenState();
}

class _DashboardShellScreenState extends State<DashboardShellScreen> {
  // Un FocusNode est toujours nécessaire pour que le listener puisse recevoir des événements.
  // En le plaçant ici avec autofocus, on maximise les chances qu'il soit actif.
  final FocusNode _focusNode = FocusNode();

  @override
  void dispose() {
    _focusNode.dispose();
    super.dispose();
  }

  void _handleKeyEvent(KeyEvent event) {
    // On s'assure de ne réagir qu'à l'événement d'appui (et non de relâchement)
    // pour éviter les doubles déclenchements.
    if (event is KeyDownEvent) {
      final String currentLocation = GoRouter.of(context).state.matchedLocation;

      // Flèche droite -> écran suivant
      if (event.logicalKey == LogicalKeyboardKey.arrowRight) {
        context.go(getNextRoute(currentLocation));
        return;
      }
      // Flèche gauche -> écran précédent
      else if (event.logicalKey == LogicalKeyboardKey.arrowLeft) {
        context.go(getPreviousRoute(currentLocation));
        return;
      }
      // Flèche bas -> Paramètres
      else if (event.logicalKey == LogicalKeyboardKey.arrowDown) {
        context.go(RouteNames.settingsRoute);
        return;
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onLongPress: () {
        context.go(RouteNames.settingsRoute);
      },
      child: KeyboardListener(
        focusNode: _focusNode,
        autofocus: true,
        onKeyEvent: _handleKeyEvent,
        child: Scaffold(
          backgroundColor: Colors.transparent,
          body: Center(
            child: ClipOval(
              child: GaugeTexturedBackground(child: widget.child),
            ),
          ),
        ),
      ),
    );
  }
}
