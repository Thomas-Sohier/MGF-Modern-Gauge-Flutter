// abstract_setting_entry.dart
import 'package:flutter/widgets.dart';

/// Interface commune à toutes les entrées de paramètres
abstract class SettingsAbstractEntry extends StatelessWidget {
  const SettingsAbstractEntry({super.key});

  /// Titre affiché sur la carte
  String get title;

  /// Le focus Node pour gérer les events
  FocusNode? get focusNode;

  /// Retourne le widget principal (slider, switch, etc.)
  Widget buildEntry(BuildContext context, bool isFocused);

  @override
  Widget build(BuildContext context) {
    return const SizedBox();
  }
}
