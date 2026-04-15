import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/ui/themes/app_text_styles.dart';
import 'package:modern_gauge_flutter/ui/widgets/settings/settings_card_shell.dart';

/// Icône + grande valeur + petit libellé, centré dans la carte.
class SettingsInfoCard extends StatelessWidget {
  final IconData icon;
  final String label;
  final String value;
  final Color? iconColor;
  final Color? valueColor;

  const SettingsInfoCard({
    super.key,
    required this.icon,
    required this.label,
    required this.value,
    this.iconColor,
    this.valueColor,
  });

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final primary = theme.primaryColor;
    return SettingsCardShell(
      child: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          Icon(icon, size: 36, color: iconColor ?? primary),
          const SizedBox(height: 14),
          Text(
            value,
            style: AppTextStyles.title.copyWith(color: valueColor),
            textAlign: TextAlign.center,
            maxLines: 2,
            overflow: TextOverflow.ellipsis,
          ),
          const SizedBox(height: 6),
          Text(
            label,
            style: AppTextStyles.small.copyWith(
              color: theme.colorScheme.onSurface.withValues(alpha: 0.5),
            ),
            textAlign: TextAlign.center,
          ),
        ],
      ),
    );
  }
}
