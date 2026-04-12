import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/providers/settings_provider.dart';
import 'package:modern_gauge_flutter/ui/screens/settings/settings_widgets.dart';
import 'package:provider/provider.dart';

/// Retourne la liste des pages Apparence.
List<Widget> buildApparencePages() => [const _ThemePage(), const _SoundPage()];

// ── Pages Apparence ─────────────────────────────────────────────────────────

class _ThemePage extends StatelessWidget {
  const _ThemePage();

  @override
  Widget build(BuildContext context) {
    return Selector<SettingsProvider, ThemeMode>(
      selector: (_, p) => p.settings.themeMode,
      builder: (context, themeMode, _) => SettingsToggleCard(
        icon: themeMode == ThemeMode.dark
            ? Icons.dark_mode_outlined
            : Icons.light_mode_outlined,
        label: 'Thème',
        value: themeMode == ThemeMode.dark,
        valueLabel: Text(themeMode == ThemeMode.dark ? 'Sombre' : 'Clair'),
        onToggle: () {
          final provider = context.read<SettingsProvider>();
          provider.setThemeMode(
            themeMode == ThemeMode.dark ? ThemeMode.light : ThemeMode.dark,
          );
        },
      ),
    );
  }
}

class _SoundPage extends StatelessWidget {
  const _SoundPage();

  @override
  Widget build(BuildContext context) {
    return Selector<SettingsProvider, bool>(
      selector: (_, p) => p.settings.soundEnabled,
      builder: (context, soundEnabled, _) => SettingsToggleCard(
        icon: Icons.volume_up_outlined,
        label: 'Son',
        value: soundEnabled,
        onToggle: () =>
            context.read<SettingsProvider>().toggleSound(!soundEnabled),
      ),
    );
  }
}
