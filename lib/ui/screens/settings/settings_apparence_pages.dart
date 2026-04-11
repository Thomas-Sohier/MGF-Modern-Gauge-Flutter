import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/providers/settings_provider.dart';
import 'package:modern_gauge_flutter/ui/screens/settings/settings_widgets.dart';
import 'package:provider/provider.dart';

/// Retourne la liste des pages Apparence.
List<Widget> buildApparencePages() => [
  const _ThemePage(),
  const _SoundPage(),
];

// ── Pages Apparence ─────────────────────────────────────────────────────────

class _ThemePage extends StatelessWidget {
  const _ThemePage();

  @override
  Widget build(BuildContext context) {
    return Consumer<SettingsProvider>(
      builder: (context, provider, _) => SettingsControlCard(
        icon: Icons.dark_mode_outlined,
        label: 'Thème sombre',
        child: Switch.adaptive(
          value: provider.settings.themeMode == ThemeMode.dark,
          onChanged: (val) =>
              provider.setThemeMode(val ? ThemeMode.dark : ThemeMode.light),
          activeThumbColor: Theme.of(context).primaryColor,
        ),
      ),
    );
  }
}

class _SoundPage extends StatelessWidget {
  const _SoundPage();

  @override
  Widget build(BuildContext context) {
    return Consumer<SettingsProvider>(
      builder: (context, provider, _) => SettingsControlCard(
        icon: Icons.volume_up_outlined,
        label: 'Son',
        child: Switch.adaptive(
          value: provider.settings.soundEnabled,
          onChanged: provider.toggleSound,
          activeThumbColor: Theme.of(context).primaryColor,
        ),
      ),
    );
  }
}
