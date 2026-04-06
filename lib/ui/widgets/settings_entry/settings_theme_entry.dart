part of './settings_entries.dart';

class SettingThemeEntry extends SettingsAbstractEntry {
  final FocusNode _focusNode;

  const SettingThemeEntry({super.key, required FocusNode focusNode})
    : _focusNode = focusNode;

  @override
  String get title => 'Thème clair/sombre';

  @override
  FocusNode get focusNode => _focusNode;
  @override
  Widget buildEntry(BuildContext context, bool isFocused) {
    final provider = Provider.of<SettingsProvider>(context);
    final isLight = provider.settings.themeMode == ThemeMode.dark;

    return SettingsCardEntry(
      title: title,
      isFocused: isFocused,
      child: Switch.adaptive(
        focusNode: focusNode,
        value: isLight,
        onChanged: (val) =>
            provider.setThemeMode(val ? ThemeMode.dark : ThemeMode.light),
        activeThumbColor: Theme.of(context).primaryColor,
      ),
    );
  }
}
