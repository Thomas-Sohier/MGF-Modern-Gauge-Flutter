import 'package:flutter/widgets.dart';

/// Gestion des entrées (OK / gauche / droite) d'une page de réglage.
///
/// Ne définit aucun GUI : uniquement des callbacks, invoqués par le handler
/// clavier central de l'écran settings (clavier physique ou websocket).
sealed class SettingsPageInput {
  const SettingsPageInput();
}

/// Mode simple : OK bascule la valeur du réglage.
class SimpleSettingsInput extends SettingsPageInput {
  final void Function(BuildContext context) onToggle;

  const SimpleSettingsInput(this.onToggle);
}

/// Mode valeur : OK capture le focus sur le réglage, gauche/droite ajustent
/// la valeur via les callbacks ; OK ou back libère le focus.
class ValueSettingsInput extends SettingsPageInput {
  final void Function(BuildContext context) onDecrease;
  final void Function(BuildContext context) onIncrease;

  const ValueSettingsInput({
    required this.onDecrease,
    required this.onIncrease,
  });
}

/// Une page de réglage : son widget et, optionnellement, sa gestion d'entrées
/// (null pour les pages en lecture seule).
class SettingsPage {
  final Widget widget;
  final SettingsPageInput? input;

  const SettingsPage(this.widget, {this.input});
}
