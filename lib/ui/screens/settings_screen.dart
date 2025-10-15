// settings_screen.dart

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:go_router/go_router.dart';
import 'package:modern_gauge_flutter/routes/route_names.dart';
import 'package:modern_gauge_flutter/ui/widgets/settings_entry/settings_abstract_entry.dart';
import 'package:modern_gauge_flutter/ui/widgets/settings_entry/settings_entries.dart';
import 'package:modern_gauge_flutter/utils/no_traversal_policy.dart';
import 'package:provider/provider.dart';
import 'package:modern_gauge_flutter/providers/settings_provider.dart';
import 'package:modern_gauge_flutter/services/settings_service.dart';

class SettingsScreen extends StatefulWidget {
  const SettingsScreen({super.key});

  @override
  State<SettingsScreen> createState() => _SettingsScreenState();
}

class _SettingsScreenState extends State<SettingsScreen> {
  static const double _itemHeight = 130.0;
  static const Duration _scrollDuration = Duration(milliseconds: 300);
  static const Curve _scrollCurve = Curves.easeInOut;

  final _scrollController = ScrollController();
  final _pageFocusNode = FocusNode();

  late final List<FocusNode> _focusNodes;
  late final List<SettingsAbstractEntry> _entries;

  int _selectedIndex = 0;
  bool _isEditing = false;

  @override
  void initState() {
    super.initState();
    _focusNodes = [];
    _entries = [
      SettingBrightnessEntry(focusNode: newFocusNode()),
      SettingThemeEntry(focusNode: newFocusNode()),
      SettingEcoEntry(focusNode: newFocusNode()),
    ];
    WidgetsBinding.instance.addPostFrameCallback((_) {
      FocusScope.of(context).requestFocus(_pageFocusNode);
      _scrollToCenter();
    });
  }

  @override
  void dispose() {
    _scrollController.dispose();
    _pageFocusNode.dispose();
    for (final node in _focusNodes) {
      node.dispose();
    }
    super.dispose();
  }

  FocusNode newFocusNode() {
    final node = FocusNode();
    _focusNodes.add(node);
    return node;
  }

  void _scrollToCenter() {
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (!_scrollController.hasClients) return;
      final viewportHeight = _scrollController.position.viewportDimension;
      final offset = (_selectedIndex * _itemHeight) - (viewportHeight / 2) + (_itemHeight / 2);
      _scrollController.animateTo(
        offset.clamp(0.0, _scrollController.position.maxScrollExtent),
        duration: _scrollDuration,
        curve: _scrollCurve,
      );
    });
  }

  void _handleKeyEvent(KeyEvent event) {
    if (event is! KeyDownEvent) return;
    if (_isEditing) {
      _handleEditingKey(event);
    } else {
      _handleNavigationKey(event);
    }
  }

  void _handleEditingKey(KeyDownEvent event) {
    if (event.logicalKey == LogicalKeyboardKey.escape || event.logicalKey == LogicalKeyboardKey.tab) {
      _exitEditMode();
    }
  }

  void _handleNavigationKey(KeyDownEvent event) {
    if (event.logicalKey == LogicalKeyboardKey.arrowDown) {
      setState(() => _selectedIndex = (_selectedIndex + 1) % _entries.length);
      _scrollToCenter();
    } else if (event.logicalKey == LogicalKeyboardKey.arrowUp) {
      setState(() => _selectedIndex = (_selectedIndex - 1).clamp(0, _entries.length - 1));
      _scrollToCenter();
    } else if (event.logicalKey == LogicalKeyboardKey.enter || event.logicalKey == LogicalKeyboardKey.select) {
      _enterEditMode();
    } else if (event.logicalKey == LogicalKeyboardKey.escape) {
      if (Navigator.canPop(context)) Navigator.pop(context);
      context.go(RouteNames.dashboardRoute + RouteNames.rpmRoute);
    }
  }

  void _enterEditMode() {
    setState(() => _isEditing = true);
    final node = _focusNodes[_selectedIndex];
    WidgetsBinding.instance.addPostFrameCallback((_) {
      FocusScope.of(context).requestFocus(node);
    });
  }

  void _exitEditMode() {
    final settingsProvider = Provider.of<SettingsProvider>(context, listen: false);
    setState(() => _isEditing = false);
    _pageFocusNode.requestFocus();
    SettingsService.saveSettings(settingsProvider.settings);
  }

  @override
  Widget build(BuildContext context) {
    return FocusTraversalGroup(
      policy: NoTraversalPolicy(),
      child: KeyboardListener(
        focusNode: _pageFocusNode,
        onKeyEvent: _handleKeyEvent,
        child: ClipOval(
          child: ListView.builder(
            controller: _scrollController,
            itemExtent: _itemHeight,
            itemCount: _entries.length,
            itemBuilder: (context, index) {
              final entry = _entries[index];
              return _SettingsCardEntry(
                title: entry.title,
                isFocused: _selectedIndex == index,
                child: entry.buildEntry(context, _isEditing),
              );
            },
          ),
        ),
      ),
    );
  }
}

class _SettingsCardEntry extends StatelessWidget {
  final String title;
  final bool isFocused;
  final Widget child;

  const _SettingsCardEntry({required this.title, required this.isFocused, required this.child});

  @override
  Widget build(BuildContext context) {
    return Card(
      color: Colors.grey[850],
      margin: const EdgeInsets.symmetric(vertical: 8.0, horizontal: 32.0),
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(8),
        side: BorderSide(color: isFocused ? Theme.of(context).primaryColor : Colors.transparent, width: 2),
      ),
      child: Padding(
        padding: const EdgeInsets.fromLTRB(16, 16, 16, 8),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(title, style: const TextStyle(color: Colors.white, fontSize: 18)),
            const SizedBox(height: 8),
            child,
          ],
        ),
      ),
    );
  }
}
