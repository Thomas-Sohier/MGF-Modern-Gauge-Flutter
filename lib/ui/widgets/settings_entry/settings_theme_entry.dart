part of './settings_entries.dart';

class SettingThemeEntry extends SettingsAbstractEntry {
  final FocusNode _focusNode;

  const SettingThemeEntry({super.key, required FocusNode focusNode}) : _focusNode = focusNode;

  @override
  String get title => 'Thème clair/sombre';

  @override
  FocusNode get focusNode => _focusNode;
  @override
  Widget buildEntry(BuildContext context, bool isEditing) {
    final provider = Provider.of<SettingsProvider>(context);
    final isLight = provider.settings.themeMode == ThemeMode.dark;

    return Switch.adaptive(
      focusNode: focusNode,
      value: isLight,
      onChanged: isEditing ? (val) => provider.setThemeMode(val ? ThemeMode.dark : ThemeMode.light) : null,
      activeThumbColor: Theme.of(context).primaryColor,
    );
  }
}
