import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/ui/themes/app_text_styles.dart';
import 'package:modern_gauge_flutter/ui/widgets/settings/settings_card_shell.dart';

/// Icône + libellé + valeur ON/OFF. Toute la carte est cliquable pour basculer.
class SettingsToggleCard extends StatelessWidget {
  final IconData icon;
  final String label;
  final bool value;
  final VoidCallback onToggle;
  final Widget? valueLabel;

  const SettingsToggleCard({
    super.key,
    required this.icon,
    required this.label,
    required this.value,
    required this.onToggle,
    this.valueLabel,
  });

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final primary = theme.primaryColor;

    return SettingsCardShell(
      child: Material(
        color: Colors.transparent,
        child: InkWell(
          onTap: onToggle,
          child: SizedBox.expand(
            child: Column(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                Icon(icon, size: 36, color: primary),
                const SizedBox(height: 14),
                Text(
                  label,
                  style: AppTextStyles.body,
                  textAlign: TextAlign.center,
                ),
                const SizedBox(height: 20),
                AnimatedDefaultTextStyle(
                  duration: const Duration(milliseconds: 200),
                  style: AppTextStyles.title.copyWith(color: primary),
                  child: valueLabel ?? Text(value ? 'ON' : 'OFF'),
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}
