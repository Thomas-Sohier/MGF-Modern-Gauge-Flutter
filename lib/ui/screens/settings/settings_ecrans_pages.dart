import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/providers/settings_provider.dart';
import 'package:modern_gauge_flutter/ui/widgets/settings_widgets.dart';
import 'package:provider/provider.dart';

/// Retourne la liste des pages Écrans (une carte par écran).
List<Widget> buildEcransPages() =>
    _kScreenDefs.map((def) => _ScreenTogglePage(def: def)).toList();

// ── Définitions des écrans ──────────────────────────────────────────────────

class _ScreenDef {
  final String routeSegment;
  final String label;
  final IconData icon;

  const _ScreenDef({
    required this.routeSegment,
    required this.label,
    required this.icon,
  });
}

const _kScreenDefs = [
  _ScreenDef(routeSegment: '/rpm', label: 'Jauges', icon: Icons.speed_rounded),
  _ScreenDef(
    routeSegment: '/time',
    label: 'Horloge',
    icon: Icons.access_time_rounded,
  ),
  _ScreenDef(
    routeSegment: '/faults',
    label: 'Codes erreurs',
    icon: Icons.warning_amber_rounded,
  ),
  _ScreenDef(
    routeSegment: '/music',
    label: 'Musique',
    icon: Icons.music_note_rounded,
  ),
];

// ── Page ────────────────────────────────────────────────────────────────────

class _ScreenTogglePage extends StatelessWidget {
  final _ScreenDef def;

  const _ScreenTogglePage({required this.def});

  @override
  Widget build(BuildContext context) {
    return Selector<SettingsProvider, Set<String>>(
      selector: (_, p) => p.settings.enabledScreens,
      builder: (context, enabledScreens, _) => SettingsToggleCard(
        icon: def.icon,
        label: def.label,
        value: enabledScreens.contains(def.routeSegment),
        onToggle: () =>
            context.read<SettingsProvider>().toggleScreen(def.routeSegment),
      ),
    );
  }
}
