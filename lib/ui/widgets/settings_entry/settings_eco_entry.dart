part of './settings_entries.dart';

class SettingEcoEntry extends SettingsAbstractEntry {
  final FocusNode _focusNode;

  const SettingEcoEntry({super.key, required FocusNode focusNode}) : _focusNode = focusNode;

  @override
  String get title => 'Mode éco';

  @override
  FocusNode get focusNode => _focusNode;

  @override
  Widget buildEntry(BuildContext context, bool isEditing) {
    final provider = Provider.of<SettingsProvider>(context);
    final isEco = true;

    return Switch.adaptive(
      focusNode: focusNode,
      value: isEco,
      onChanged: isEditing ? (val) => false : null,
      activeThumbColor: Theme.of(context).primaryColor,
    );
  }
}
