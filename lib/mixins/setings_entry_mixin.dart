// Fichier: setting_entry_mixin.dart

import 'package:flutter/widgets.dart';

/// Mixin pour aider à créer de nouvelles entrées de paramètres
/// Fournit la gestion standard du focus et de l'AbsorbPointer
mixin SettingEntryMixin {
  /// Enveloppe un widget enfant avec AbsorbPointer et Focus
  /// pour la gestion automatique des interactions
  static Widget wrapWithInputHandling({
    required Widget child,
    required bool isEditing,
    required BuildContext context,
  }) {
    return AbsorbPointer(
      absorbing: !isEditing,
      child: Focus(
        skipTraversal: true,
        canRequestFocus: isEditing,
        onFocusChange: (hasFocus) {
          if (!isEditing && hasFocus) {
            FocusScope.of(context).unfocus();
          }
        },
        child: child,
      ),
    );
  }
}
