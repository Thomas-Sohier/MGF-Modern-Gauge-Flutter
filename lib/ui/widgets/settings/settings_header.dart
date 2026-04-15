import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/ui/themes/app_text_styles.dart';

class SettingsHeader extends StatelessWidget {
  final String title;
  final VoidCallback onBack;
  final double height;

  const SettingsHeader({
    super.key,
    required this.title,
    required this.onBack,
    this.height = 100,
  });

  @override
  Widget build(BuildContext context) {
    final color = Theme.of(context).primaryColor;
    return Material(
      color: Colors.transparent,
      child: InkWell(
        onTap: onBack,
        child: SizedBox(
          height: height,
          child: Row(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              Icon(Icons.arrow_back_ios_new_rounded, color: color, size: 22),
              const SizedBox(width: 8),
              Text(title, style: AppTextStyles.title.copyWith(color: color)),
            ],
          ),
        ),
      ),
    );
  }
}
