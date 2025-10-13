part of './settings_entries.dart';

class SettingToggleEntry extends StatelessWidget {
  final FocusNode focusNode;
  final bool isEditing;
  final bool value;
  final ValueChanged<bool> onChanged;

  const SettingToggleEntry({
    super.key,
    required this.focusNode,
    required this.isEditing,
    required this.value,
    required this.onChanged,
  });

  @override
  Widget build(BuildContext context) {
    return SettingEntryMixin.wrapWithInputHandling(
      isEditing: isEditing,
      context: context,
      child: Checkbox(
        focusNode: focusNode,
        value: value,
        onChanged: isEditing ? (val) => onChanged(val ?? false) : null,
        activeColor: Theme.of(context).primaryColor,
      ),
    );
  }
}
