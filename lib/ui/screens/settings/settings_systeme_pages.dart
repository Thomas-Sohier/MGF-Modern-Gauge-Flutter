import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/providers/ecu_provider.dart';
import 'package:modern_gauge_flutter/providers/settings_provider.dart';
import 'package:modern_gauge_flutter/ui/widgets/settings_widgets.dart';
import 'package:provider/provider.dart';

/// Retourne la liste des pages Système.
List<Widget> buildSystemePages() => [const _WifiPage()];

// ── Pages Système ───────────────────────────────────────────────────────────

/// Bascule WiFi : pilote la radio de l'appareil via le serveur Go (rfkill) et
/// mémorise le choix dans les réglages.
class _WifiPage extends StatelessWidget {
  const _WifiPage();

  @override
  Widget build(BuildContext context) {
    return Consumer<SettingsProvider>(
      builder: (context, settings, _) {
        final enabled = settings.settings.wifiEnabled;
        return SettingsToggleCard(
          icon: enabled ? Icons.wifi_rounded : Icons.wifi_off_rounded,
          label: 'WiFi',
          value: enabled,
          onToggle: () {
            final next = !enabled;
            settings.setWifiEnabled(next);
            context.read<EcuProvider>().setWifiEnabled(next);
          },
        );
      },
    );
  }
}
