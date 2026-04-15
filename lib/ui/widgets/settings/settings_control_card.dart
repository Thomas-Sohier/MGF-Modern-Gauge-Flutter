import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/ui/themes/app_text_styles.dart';
import 'package:modern_gauge_flutter/ui/widgets/settings/settings_card_shell.dart';

/// Icône + libellé + widget de contrôle (Switch, Slider…), centré dans la carte.
class SettingsControlCard extends StatelessWidget {
  final IconData icon;
  final String label;
  final Widget child;

  const SettingsControlCard({
    super.key,
    required this.icon,
    required this.label,
    required this.child,
  });

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final primary = theme.primaryColor;
    return SettingsCardShell(
      child: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          Icon(icon, size: 36, color: primary),
          const SizedBox(height: 14),
          Text(
            label,
            style: AppTextStyles.small.copyWith(
              color: theme.colorScheme.onSurface,
            ),
            textAlign: TextAlign.center,
          ),
          const SizedBox(height: 20),
          child,
        ],
      ),
    );
  }
}
