// Fichier: setting_slider_entry.dart (part of settings_entries.dart)

part of './settings_entries.dart';

/// Entrée slider pour les paramètres numériques
class SettingSliderEntry extends StatelessWidget {
  final FocusNode focusNode;
  final bool isEditing;
  final double value;
  final double min;
  final double max;
  final int divisions;
  final ValueChanged<double> onChanged;

  const SettingSliderEntry({
    super.key,
    required this.focusNode,
    required this.isEditing,
    required this.value,
    this.min = 0.1,
    this.max = 1.0,
    this.divisions = 9,
    required this.onChanged,
  });

  @override
  Widget build(BuildContext context) {
    return SettingEntryMixin.wrapWithInputHandling(
      isEditing: isEditing,
      context: context,
      child: Slider(
        focusNode: focusNode,
        value: value,
        min: min,
        max: max,
        divisions: divisions,
        label: '${(value * 100).round()}%',
        onChanged: (double val) {
          if (isEditing) onChanged(val);
        },
        activeColor: Theme.of(context).primaryColor,
        inactiveColor: Colors.grey[600],
      ),
    );
  }
}
