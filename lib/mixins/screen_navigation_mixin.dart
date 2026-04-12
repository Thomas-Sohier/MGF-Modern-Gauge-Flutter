import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/providers/settings_provider.dart';
import 'package:provider/provider.dart';

/// Un mixin pour ajouter une navigation par gestes (tap/swipe) à un écran.
///
/// Doit être utilisé sur la classe State d'un StatefulWidget.
mixin ScreenNavigationMixin<T extends StatefulWidget> on State<T> {
  void nextScreen();

  void previousScreen();

  /// Retourne les segments d'écrans activés depuis les settings.
  Set<String> get enabledScreens =>
      context.read<SettingsProvider>().settings.enabledScreens;

  Widget buildNavigableScreen({required Widget child}) {
    return GestureDetector(
      onTapUp: (details) {
        if (details.globalPosition.dx > MediaQuery.of(context).size.width / 2) {
          nextScreen();
        } else {
          previousScreen();
        }
      },
      onHorizontalDragEnd: (details) {
        if (details.primaryVelocity == null ||
            details.primaryVelocity!.abs() < 100) {
          return;
        }
        if (details.primaryVelocity! < 0) {
          nextScreen();
        } else if (details.primaryVelocity! > 0) {
          previousScreen();
        }
      },
      behavior: HitTestBehavior.opaque,
      child: child,
    );
  }
}
