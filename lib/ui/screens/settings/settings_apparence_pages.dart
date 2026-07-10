import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/providers/settings_provider.dart';
import 'package:modern_gauge_flutter/ui/screens/settings/settings_page_input.dart';
import 'package:modern_gauge_flutter/ui/widgets/settings_widgets.dart';
import 'package:provider/provider.dart';

/// Retourne la liste des pages Apparence.
List<SettingsPage> buildApparencePages() => const [
  SettingsPage(_ThemePage(), input: SimpleSettingsInput(_toggleTheme)),
];

void _toggleTheme(BuildContext context) {
  final provider = context.read<SettingsProvider>();
  provider.setThemeMode(
    provider.settings.themeMode == ThemeMode.dark
        ? ThemeMode.light
        : ThemeMode.dark,
  );
}

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
        onToggle: () => _toggleTheme(context),
      ),
    );
  }
}
