part of 'settings_entries.dart';

class SettingsCardEntry extends StatelessWidget {
  final String title;
  final bool isFocused;
  final Widget child;

  const SettingsCardEntry({super.key, required this.title, required this.isFocused, required this.child});

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Card(
      color: theme.cardColor,
      elevation: 4,
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(8),
        side: BorderSide(color: isFocused ? theme.primaryColor : Colors.transparent, width: 2),
      ),
      child: Padding(
        padding: const EdgeInsets.fromLTRB(16, 16, 16, 8),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(title, style: const TextStyle(fontSize: 18)),
            const SizedBox(height: 8),
            child,
          ],
        ),
      ),
    );
  }
}
