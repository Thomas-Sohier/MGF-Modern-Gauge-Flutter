import 'package:flutter/material.dart';

class SettingsNavBar extends StatelessWidget {
  final int index;
  final int total;
  final double height;
  final VoidCallback? onPrev;
  final VoidCallback? onNext;

  const SettingsNavBar({
    super.key,
    required this.index,
    required this.total,
    required this.onPrev,
    required this.onNext,
    this.height = 100,
  });

  @override
  Widget build(BuildContext context) {
    final color = Theme.of(context).primaryColor;
    return SizedBox(
      height: height,
      child: Stack(
        children: [
          Positioned(
            top: 5,
            left: 0,
            right: 0,
            child: Row(
              mainAxisAlignment: MainAxisAlignment.center,
              crossAxisAlignment: CrossAxisAlignment.center,
              children: [
                ...List.generate(total, (i) {
                  final active = i == index;
                  return AnimatedContainer(
                    duration: const Duration(milliseconds: 200),
                    margin: const EdgeInsets.symmetric(horizontal: 3),
                    width: active ? 10 : 6,
                    height: active ? 10 : 6,
                    decoration: BoxDecoration(
                      color: color.withValues(alpha: active ? 1.0 : 0.3),
                      shape: BoxShape.circle,
                    ),
                  );
                }),
              ],
            ),
          ),
          Row(
            mainAxisAlignment: MainAxisAlignment.center,
            spacing: 8,
            children: [
              Expanded(
                child: SettingsNavArrow(
                  icon: Icons.chevron_left_rounded,
                  onTap: onPrev,
                  alignment: Alignment.centerRight,
                ),
              ),
              Expanded(
                child: SettingsNavArrow(
                  icon: Icons.chevron_right_rounded,
                  onTap: onNext,
                  alignment: Alignment.centerLeft,
                ),
              ),
            ],
          ),
        ],
      ),
    );
  }
}

class SettingsNavArrow extends StatelessWidget {
  final IconData icon;
  final VoidCallback? onTap;
  final Alignment alignment;

  const SettingsNavArrow({
    super.key,
    required this.icon,
    required this.onTap,
    this.alignment = Alignment.center,
  });

  @override
  Widget build(BuildContext context) {
    final color = Theme.of(context).primaryColor;
    return Material(
      color: Colors.transparent,
      child: InkWell(
        onTap: onTap,
        child: SizedBox(
          height: double.infinity,
          width: double.infinity,
          child: AnimatedOpacity(
            duration: const Duration(milliseconds: 150),
            opacity: onTap != null ? 1.0 : 0.2,
            child: Align(
              alignment: alignment,
              child: Icon(icon, color: color, size: 32),
            ),
          ),
        ),
      ),
    );
  }
}
