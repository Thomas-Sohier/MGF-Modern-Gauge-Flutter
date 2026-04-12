import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/providers/ecu_provider.dart';
import 'package:modern_gauge_flutter/ui/screens/settings/settings_widgets.dart';
import 'package:provider/provider.dart';

/// Retourne la liste des pages ECU (5 cartes info en lecture seule).
List<Widget> buildEcuPages() => [
  const _EcuConnexionPage(),
  const _EcuTypePage(),
  const _EcuAgentVersionPage(),
  const _EcuSerialPortPage(),
  const _EcuFaultsPage(),
];

// ── Pages ECU ───────────────────────────────────────────────────────────────

class _EcuConnexionPage extends StatelessWidget {
  const _EcuConnexionPage();

  @override
  Widget build(BuildContext context) {
    return Consumer<EcuProvider>(
      builder: (context, provider, _) {
        final connected = provider.currentData.connected;
        final color = connected
            ? Colors.green
            : Theme.of(context).colorScheme.error;
        return SettingsInfoCard(
          icon: connected ? Icons.link_rounded : Icons.link_off_rounded,
          label: 'Connecté',
          value: connected ? 'Oui' : 'Non',
          iconColor: color,
          valueColor: color,
        );
      },
    );
  }
}

class _EcuTypePage extends StatelessWidget {
  const _EcuTypePage();

  @override
  Widget build(BuildContext context) {
    return Consumer<EcuProvider>(
      builder: (context, provider, _) => SettingsInfoCard(
        icon: Icons.memory_rounded,
        label: 'Type ECU',
        value: provider.currentData.ecuType ?? '—',
      ),
    );
  }
}

class _EcuAgentVersionPage extends StatelessWidget {
  const _EcuAgentVersionPage();

  @override
  Widget build(BuildContext context) {
    return Consumer<EcuProvider>(
      builder: (context, provider, _) => SettingsInfoCard(
        icon: Icons.tag_rounded,
        label: 'Version agent',
        value: provider.currentData.agentVersion ?? '—',
      ),
    );
  }
}

class _EcuSerialPortPage extends StatelessWidget {
  const _EcuSerialPortPage();

  @override
  Widget build(BuildContext context) {
    return Consumer<EcuProvider>(
      builder: (context, provider, _) => SettingsInfoCard(
        icon: Icons.usb_rounded,
        label: 'Port série',
        value: provider.currentData.selectedSerialPort ?? '—',
      ),
    );
  }
}

class _EcuFaultsPage extends StatelessWidget {
  const _EcuFaultsPage();

  @override
  Widget build(BuildContext context) {
    return Consumer<EcuProvider>(
      builder: (context, provider, _) {
        final count = provider.currentData.faults?.length ?? 0;
        final color = count > 0 ? Colors.orange : null;
        return SettingsInfoCard(
          icon: Icons.warning_amber_rounded,
          label: 'Codes erreur',
          value: '$count',
          iconColor: color,
          valueColor: color,
        );
      },
    );
  }
}
