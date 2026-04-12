import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/ui/themes/app_text_styles.dart';

// ── Header ─────────────────────────────────────────────────────────────────

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
              Text(
                title,
                style: TextStyle(
                  color: color,
                  fontSize: 18,
                  fontWeight: FontWeight.bold,
                  letterSpacing: 1.5,
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

// ── Barre de navigation (points + flèches) ──────────────────────────────────

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

// ── Shell de carte (fond + bordure arrondie) ───────────────────────────────

/// Conteneur qui occupe tout l'espace disponible avec fond et bordure.
class SettingsCardShell extends StatelessWidget {
  final Widget child;

  const SettingsCardShell({super.key, required this.child});

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return SizedBox(
      width: double.infinity,
      child: DecoratedBox(
        decoration: BoxDecoration(
          border: Border.symmetric(
            horizontal: BorderSide(
              color: theme.colorScheme.outline.withValues(alpha: 0.15),
            ),
          ),
        ),
        child: child,
      ),
    );
  }
}

// ── Carte info (lecture seule) ─────────────────────────────────────────────

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

// ── Carte contrôle (interactive) ───────────────────────────────────────────

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
              color: theme.colorScheme.onSurface.withValues(alpha: 0.5),
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

// ── Carte toggle (boolean, carte entière cliquable) ────────────────────────

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
    final valueColor = primary;

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
                  style: AppTextStyles.body.copyWith(
                    color: theme.colorScheme.onSurface.withValues(alpha: 0.5),
                  ),
                  textAlign: TextAlign.center,
                ),
                const SizedBox(height: 20),
                AnimatedDefaultTextStyle(
                  duration: const Duration(milliseconds: 200),
                  style: TextStyle(
                    fontFamily: 'JetBrainsMono',
                    fontSize: 22,
                    fontWeight: FontWeight.w700,
                    color: valueColor,
                    letterSpacing: 2,
                  ),
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
