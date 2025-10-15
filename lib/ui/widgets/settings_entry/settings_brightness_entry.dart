part of './settings_entries.dart';

class SettingBrightnessEntry extends SettingsAbstractEntry {
  final FocusNode _focusNode;

  const SettingBrightnessEntry({super.key, required FocusNode focusNode}) : _focusNode = focusNode;

  @override
  String get title => 'Luminosité';

  @override
  FocusNode get focusNode => _focusNode;

  @override
  Widget buildEntry(BuildContext context, bool isFocused, bool isEditing) {
    final provider = Provider.of<SettingsProvider>(context);

    return SettingsCardEntry(
      title: title,
      isFocused: isFocused,
      child: Slider(
        focusNode: focusNode,
        value: provider.settings.screenBrightness,
        min: 0.1,
        max: 1.0,
        divisions: 9,
        label: '${(provider.settings.screenBrightness * 100).round()}%',
        onChanged: isEditing ? (val) => provider.setScreenBrightness(val) : null,
        activeColor: Theme.of(context).primaryColor,
        inactiveColor: Colors.grey[600],
      ),
    );
  }
}
