// Fichier: settings_screen.dart

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:modern_gauge_flutter/ui/widgets/settings_entry/settings_entries.dart';
import 'package:provider/provider.dart';

import 'package:modern_gauge_flutter/providers/settings_provider.dart';
import 'package:modern_gauge_flutter/services/settings_service.dart';

class SettingsScreen extends StatefulWidget {
  const SettingsScreen({super.key});

  @override
  State<SettingsScreen> createState() => _SettingsScreenState();
}

class _SettingsScreenState extends State<SettingsScreen> {
  // --- CONSTANTS ---
  static const double _itemHeight = 130.0;
  static const int _settingsCount = 5;
  static const Duration _scrollDuration = Duration(milliseconds: 300);
  static const Curve _scrollCurve = Curves.easeInOut;

  // --- LATE FINAL ---
  late final ScrollController _scrollController;
  late final FocusNode _pageFocusNode;
  late final List<FocusNode> _itemFocusNodes;
  late final List<_SettingItemConfig> _settingConfigs;

  // --- STATE ---
  int _selectedIndex = 0;
  bool _isEditing = false;

  @override
  void initState() {
    super.initState();

    _scrollController = ScrollController();
    _pageFocusNode = FocusNode();
    _itemFocusNodes = List.generate(_settingsCount, (_) => FocusNode());
    _settingConfigs = List.generate(
      _settingsCount,
      (index) => _SettingItemConfig(
        title: 'Luminosité #$index',
        focusNode: _itemFocusNodes[index],
      ),
    );

    WidgetsBinding.instance.addPostFrameCallback((_) {
      FocusScope.of(context).requestFocus(_pageFocusNode);
      _scrollToCenter();
    });
  }

  @override
  void dispose() {
    _scrollController.dispose();
    _pageFocusNode.dispose();
    for (var node in _itemFocusNodes) {
      node.dispose();
    }
    super.dispose();
  }

  void _scrollToCenter() {
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (!_scrollController.hasClients) return;

      final viewportHeight = _scrollController.position.viewportDimension;
      final targetScrollOffset =
          (_selectedIndex * _itemHeight) -
          (viewportHeight / 2) +
          (_itemHeight / 2);

      final clampedOffset = targetScrollOffset.clamp(
        0.0,
        _scrollController.position.maxScrollExtent,
      );

      _scrollController.animateTo(
        clampedOffset,
        duration: _scrollDuration,
        curve: _scrollCurve,
      );
    });
  }

  void _handleKeyEvent(KeyEvent event) {
    if (event is! KeyDownEvent) return;

    if (_isEditing) {
      _handleEditingKeyEvent(event);
    } else {
      _handleNavigationKeyEvent(event);
    }
  }

  void _handleEditingKeyEvent(KeyEvent event) {
    if (event.logicalKey == LogicalKeyboardKey.escape ||
        event.logicalKey == LogicalKeyboardKey.tab) {
      _exitEditMode();
    }
  }

  void _handleNavigationKeyEvent(KeyEvent event) {
    if (event.logicalKey == LogicalKeyboardKey.arrowDown) {
      _moveDown();
    } else if (event.logicalKey == LogicalKeyboardKey.arrowUp) {
      _moveUp();
    } else if (event.logicalKey == LogicalKeyboardKey.enter ||
        event.logicalKey == LogicalKeyboardKey.select) {
      _enterEditMode();
    } else if (event.logicalKey == LogicalKeyboardKey.escape) {
      if (Navigator.canPop(context)) Navigator.pop(context);
    }
  }

  void _moveDown() {
    setState(() {
      _selectedIndex = (_selectedIndex + 1).clamp(0, _settingsCount - 1);
    });
    _scrollToCenter();
  }

  void _moveUp() {
    setState(() {
      _selectedIndex = (_selectedIndex - 1).clamp(0, _settingsCount - 1);
    });
    _scrollToCenter();
  }

  void _enterEditMode() {
    setState(() => _isEditing = true);
    _itemFocusNodes[_selectedIndex].requestFocus();
  }

  void _exitEditMode() {
    final settingsProvider = Provider.of<SettingsProvider>(
      context,
      listen: false,
    );
    setState(() => _isEditing = false);
    _pageFocusNode.requestFocus();
    SettingsService.saveSettings(settingsProvider.settings);
  }

  @override
  Widget build(BuildContext context) {
    return KeyboardListener(
      focusNode: _pageFocusNode,
      onKeyEvent: _handleKeyEvent,
      child: AspectRatio(
        aspectRatio: 1,
        child: ClipOval(
          child: ListView.builder(
            controller: _scrollController,
            itemExtent: _SettingsScreenState._itemHeight,
            itemCount: _settingsCount,
            itemBuilder: (context, index) {
              return _SettingItemWidget(
                config: _settingConfigs[index],
                isFocused: index == _selectedIndex,
                isEditing: _isEditing,
              );
            },
          ),
        ),
      ),
    );
  }
}

/// Configuration immuable d'un élément de paramètre
class _SettingItemConfig {
  final String title;
  final FocusNode focusNode;

  const _SettingItemConfig({required this.title, required this.focusNode});
}

/// Widget pour un élément de paramètre
class _SettingItemWidget extends StatelessWidget {
  final _SettingItemConfig config;
  final bool isFocused;
  final bool isEditing;

  const _SettingItemWidget({
    required this.config,
    required this.isFocused,
    required this.isEditing,
  });

  @override
  Widget build(BuildContext context) {
    return _SettingsCardEntry(
      isFocused: isFocused,
      title: config.title,
      child: Consumer<SettingsProvider>(
        builder: (context, settingsProvider, _) {
          return SettingSliderEntry(
            value: settingsProvider.settings.screenBrightness,
            focusNode: config.focusNode,
            isEditing: isEditing,
            onChanged: (value) {
              Provider.of<SettingsProvider>(
                context,
                listen: false,
              ).setScreenBrightness(value);
            },
          );
        },
      ),
    );
  }
}

class _SettingsCardEntry extends StatelessWidget {
  final bool isFocused;
  final String title;
  final Widget child;

  const _SettingsCardEntry({
    required this.isFocused,
    required this.title,
    required this.child,
  });

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: _SettingsScreenState._itemHeight,
      child: Card(
        color: Colors.grey[850],
        margin: const EdgeInsets.symmetric(vertical: 8.0, horizontal: 32.0),
        shape: RoundedRectangleBorder(
          side: BorderSide(
            color: isFocused
                ? Theme.of(context).primaryColor
                : Colors.transparent,
            width: 2,
          ),
          borderRadius: BorderRadius.circular(8.0),
        ),
        child: Padding(
          padding: const EdgeInsets.fromLTRB(16, 16, 16, 8),
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(
                title,
                style: const TextStyle(color: Colors.white, fontSize: 18),
              ),
              child,
            ],
          ),
        ),
      ),
    );
  }
}
