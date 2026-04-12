import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/ui/widgets/settings_widgets.dart';

/// Retourne la liste des pages Système.
List<Widget> buildSystemePages() => [const _EmptyPage()];

// ── Pages Système ───────────────────────────────────────────────────────────

class _EmptyPage extends StatelessWidget {
  const _EmptyPage();

  @override
  Widget build(BuildContext context) {
    return SettingsControlCard(
      icon: Icons.dangerous_outlined,
      label: 'Rien ici',
      child: const SizedBox.shrink(),
    );
  }
}
