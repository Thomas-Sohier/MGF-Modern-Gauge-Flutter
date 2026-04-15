import 'package:flutter/material.dart';

/// Conteneur qui occupe tout l'espace disponible avec fond et bordure.
class SettingsCardShell extends StatelessWidget {
  final Widget child;

  const SettingsCardShell({super.key, required this.child});

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return SizedBox(
      width: double.infinity,
      child: DecoratedBox(
        decoration: BoxDecoration(
          border: Border.symmetric(
            horizontal: BorderSide(color: theme.colorScheme.outline),
          ),
        ),
        child: child,
      ),
    );
  }
}
