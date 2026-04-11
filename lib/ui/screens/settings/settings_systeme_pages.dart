import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/providers/settings_provider.dart';
import 'package:modern_gauge_flutter/ui/screens/settings/settings_widgets.dart';
import 'package:provider/provider.dart';

/// Retourne la liste des pages Système.
List<Widget> buildSystemePages() => [const _SleepDelayPage()];

// ── Pages Système ───────────────────────────────────────────────────────────

class _SleepDelayPage extends StatelessWidget {
  const _SleepDelayPage();

  static const _delays = [60, 120, 300, 600, 900];

  static String _label(int seconds) =>
      seconds < 60 ? '${seconds}s' : '${seconds ~/ 60}min';

  @override
  Widget build(BuildContext context) {
    return Consumer<SettingsProvider>(
      builder: (context, provider, _) {
        final current = provider.settings.autoSleepDelaySeconds;
        final primary = Theme.of(context).primaryColor;
        return SettingsControlCard(
          icon: Icons.bedtime_outlined,
          label: 'Mise en veille',
          child: Row(
            mainAxisAlignment: MainAxisAlignment.center,
            children: _delays.map((delay) {
              final active = current == delay;
              return GestureDetector(
                onTap: () => provider.setAutoSleepDelay(delay),
                child: AnimatedContainer(
                  duration: const Duration(milliseconds: 200),
                  margin: const EdgeInsets.symmetric(horizontal: 3),
                  padding: const EdgeInsets.symmetric(
                    horizontal: 7,
                    vertical: 5,
                  ),
                  decoration: BoxDecoration(
                    color: active ? primary : Colors.transparent,
                    borderRadius: BorderRadius.circular(8),
                    border: Border.all(
                      color: active ? primary : primary.withValues(alpha: 0.35),
                    ),
                  ),
                  child: Text(
                    _label(delay),
                    style: TextStyle(
                      fontFamily: 'JetBrainsMono',
                      fontSize: 12,
                      fontWeight: active ? FontWeight.bold : FontWeight.normal,
                      color: active
                          ? Theme.of(context).colorScheme.onPrimary
                          : primary,
                    ),
                  ),
                ),
              );
            }).toList(),
          ),
        );
      },
    );
  }
}
