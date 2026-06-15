import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/providers/ecu_provider.dart';
import 'package:modern_gauge_flutter/providers/settings_provider.dart';
import 'package:modern_gauge_flutter/ui/themes/app_text_styles.dart';
import 'package:modern_gauge_flutter/ui/widgets/settings_widgets.dart';
import 'package:modern_gauge_flutter/utils/app_constants.dart';
import 'package:provider/provider.dart';

/// Retourne la liste des pages Système.
List<Widget> buildSystemePages() => const [
  _WifiPage(),
  _NotificationDurationPage(),
];

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

/// Durée d'affichage de l'écran de notification entrante (secondes).
class _NotificationDurationPage extends StatelessWidget {
  const _NotificationDurationPage();

  @override
  Widget build(BuildContext context) {
    return Selector<SettingsProvider, int>(
      selector: (_, p) => p.settings.notificationDurationSeconds,
      builder: (context, seconds, _) {
        final provider = context.read<SettingsProvider>();
        return SettingsControlCard(
          icon: Icons.notifications_active_outlined,
          label: 'Notifications',
          child: _Stepper(
            value: seconds,
            min: AppConstants.minNotificationDurationSeconds,
            max: AppConstants.maxNotificationDurationSeconds,
            suffix: 's',
            onChanged: provider.setNotificationDurationSeconds,
          ),
        );
      },
    );
  }
}

/// Sélecteur −/+ pour une valeur entière bornée.
class _Stepper extends StatelessWidget {
  final int value;
  final int min;
  final int max;
  final String suffix;
  final ValueChanged<int> onChanged;

  const _Stepper({
    required this.value,
    required this.min,
    required this.max,
    required this.suffix,
    required this.onChanged,
  });

  @override
  Widget build(BuildContext context) {
    final primary = Theme.of(context).primaryColor;
    return Row(
      mainAxisAlignment: MainAxisAlignment.center,
      children: [
        IconButton(
          icon: const Icon(Icons.remove_circle_outline),
          color: primary,
          onPressed: value > min ? () => onChanged(value - 1) : null,
        ),
        SizedBox(
          width: 64,
          child: Text(
            '$value $suffix',
            textAlign: TextAlign.center,
            style: AppTextStyles.body.copyWith(fontWeight: FontWeight.bold),
          ),
        ),
        IconButton(
          icon: const Icon(Icons.add_circle_outline),
          color: primary,
          onPressed: value < max ? () => onChanged(value + 1) : null,
        ),
      ],
    );
  }
}
