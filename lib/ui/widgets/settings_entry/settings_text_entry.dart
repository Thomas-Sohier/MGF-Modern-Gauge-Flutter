// Fichier: setting_text_entry.dart (part of settings_entries.dart)

part of './settings_entries.dart';

/// Entrée textuelle pour les paramètres string
class SettingTextEntry extends StatefulWidget {
  final FocusNode focusNode;
  final bool isEditing;
  final String initialValue;
  final ValueChanged<String> onChanged;

  const SettingTextEntry({
    super.key,
    required this.focusNode,
    required this.isEditing,
    required this.initialValue,
    required this.onChanged,
  });

  @override
  State<SettingTextEntry> createState() => _SettingTextEntryState();
}

class _SettingTextEntryState extends State<SettingTextEntry> {
  late final TextEditingController _controller;

  @override
  void initState() {
    super.initState();
    _controller = TextEditingController(text: widget.initialValue);
  }

  @override
  void didUpdateWidget(SettingTextEntry oldWidget) {
    super.didUpdateWidget(oldWidget);
    // Mettre à jour le contrôleur si initialValue change
    if (oldWidget.initialValue != widget.initialValue) {
      _controller.text = widget.initialValue;
    }
  }

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return SettingEntryMixin.wrapWithInputHandling(
      isEditing: widget.isEditing,
      context: context,
      child: TextField(
        controller: _controller,
        focusNode: widget.focusNode,
        onChanged: widget.onChanged,
        enabled: widget.isEditing,
        style: const TextStyle(color: Colors.white),
        decoration: InputDecoration(
          border: OutlineInputBorder(borderRadius: BorderRadius.circular(8.0)),
          contentPadding: const EdgeInsets.symmetric(
            horizontal: 12,
            vertical: 8,
          ),
        ),
      ),
    );
  }
}
