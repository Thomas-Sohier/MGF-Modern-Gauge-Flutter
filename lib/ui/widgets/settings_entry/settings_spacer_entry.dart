part of 'settings_entries.dart';

class SettingSpacerEntry extends SettingsAbstractEntry {
  final double? height;
  const SettingSpacerEntry({super.key, required this.height});

  @override
  String get title => '';

  @override
  FocusNode? get focusNode => null;

  @override
  Widget buildEntry(BuildContext context, bool isFocused) {
    return SizedBox(height: height);
  }
}
