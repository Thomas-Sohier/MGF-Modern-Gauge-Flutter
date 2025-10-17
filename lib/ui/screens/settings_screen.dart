// settings_screen.dart

import 'dart:math';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:go_router/go_router.dart';
import 'package:modern_gauge_flutter/providers/settings_provider.dart';
import 'package:modern_gauge_flutter/routes/route_names.dart';
import 'package:modern_gauge_flutter/services/settings_service.dart';
import 'package:modern_gauge_flutter/ui/widgets/settings_entry/settings_abstract_entry.dart';
import 'package:modern_gauge_flutter/ui/widgets/settings_entry/settings_entries.dart';
import 'package:modern_gauge_flutter/utils/no_traversal_policy.dart';
import 'package:provider/provider.dart';

class SettingsScreen extends StatefulWidget {
  const SettingsScreen({super.key});

  @override
  State<SettingsScreen> createState() => _SettingsScreenState();
}

class _SettingsScreenState extends State<SettingsScreen> {
  static const Duration _animationDuration = Duration(milliseconds: 200);
  static const Curve _animationCurve = Curves.easeInOut;

  static const double _itemHeight = 120.0;
  static const Duration _scrollDuration = Duration(milliseconds: 300);
  static const Curve _scrollCurve = Curves.easeInOut;

  final _scrollController = ScrollController();
  final _pageFocusNode = FocusNode();

  late final List<FocusNode> _focusNodes;
  late final List<SettingsAbstractEntry> _entries;
  late final List<MapEntry<int, SettingsAbstractEntry>> _selectableEntries;

  int _selectedIndex = 0;
  bool _isEditing = false;

  @override
  void initState() {
    super.initState();
    _focusNodes = [];
    _entries = [
      SettingSpacerEntry(height: 100),
      SettingBrightnessEntry(focusNode: newFocusNode()),
      SettingThemeEntry(focusNode: newFocusNode()),
      SettingEcoEntry(focusNode: newFocusNode()),
    ];

    // On crée une liste filtrée des widgets qui ont un focusNode.
    // On garde l'index original pour la logique de focus.
    _selectableEntries = _entries.asMap().entries.where((entry) => entry.value.focusNode != null).toList();

    _selectedIndex = _findNextSelectableIndex(-1);
    if (_selectedIndex == -1) _selectedIndex = 0;

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
    final node = FocusNode(skipTraversal: true);
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

  // --- LOGIQUE DE NAVIGATION MISE À JOUR ---
  int _findNextSelectableIndex(int currentIndex) {
    int nextIndex = currentIndex + 1;
    while (nextIndex < _entries.length) {
      if (_entries[nextIndex].focusNode != null) {
        return nextIndex;
      }
      nextIndex++;
    }
    return currentIndex;
  }

  int _findPreviousSelectableIndex(int currentIndex) {
    int prevIndex = currentIndex - 1;
    while (prevIndex >= 0) {
      if (_entries[prevIndex].focusNode != null) {
        return prevIndex;
      }
      prevIndex--;
    }
    return currentIndex;
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
      setState(() {
        _selectedIndex = _findNextSelectableIndex(_selectedIndex);
      });
      _scrollToCenter();
    } else if (event.logicalKey == LogicalKeyboardKey.arrowUp) {
      setState(() {
        _selectedIndex = _findPreviousSelectableIndex(_selectedIndex);
      });
      _scrollToCenter();
    } else if (event.logicalKey == LogicalKeyboardKey.enter || event.logicalKey == LogicalKeyboardKey.select) {
      _enterEditMode();
    } else if (event.logicalKey == LogicalKeyboardKey.escape) {
      if (Navigator.canPop(context)) Navigator.pop(context);
      context.go(RouteNames.dashboardRoute + RouteNames.rpmRoute);
    }
  }

  void _enterEditMode() {
    final currentEntry = _entries[_selectedIndex];
    if (currentEntry.focusNode == null) return;

    setState(() => _isEditing = true);
    WidgetsBinding.instance.addPostFrameCallback((_) {
      FocusScope.of(context).requestFocus(currentEntry.focusNode);
    });
  }

  void _exitEditMode() {
    final settingsProvider = Provider.of<SettingsProvider>(context, listen: false);
    setState(() => _isEditing = false);
    _pageFocusNode.requestFocus();
    SettingsService().saveSettings(settingsProvider.settings);
  }

  // --- RENDU MIS À JOUR ---
  @override
  Widget build(BuildContext context) {
    return Material(
      color: Colors.transparent,
      child: Center(
        // On centre le cercle dans l'espace disponible
        child: ClipOval(
          child: AspectRatio(
            aspectRatio: 1,
            child: ColoredBox(
              color: Theme.of(context).scaffoldBackgroundColor,
              child: KeyboardListener(
                focusNode: _pageFocusNode,
                onKeyEvent: _handleKeyEvent,
                child: FocusTraversalGroup(
                  policy: NoTraversalPolicy(),
                  child: LayoutBuilder(
                    builder: (context, constraints) {
                      final List<Widget> transformedItems = [];
                      final List<Map<String, dynamic>> calculatedItems = [];
                      final int displaySelectedIndex = _selectableEntries.indexWhere(
                        (entry) => entry.key == _selectedIndex,
                      );
                      if (displaySelectedIndex == -1) return const SizedBox.shrink();

                      for (int i = 0; i < _selectableEntries.length; i++) {
                        final entry = _selectableEntries[i].value;
                        final int distance = i - displaySelectedIndex;
                        final bool isSelected = distance == 0;

                        final double height = constraints.maxHeight * 0.5;
                        final double verticalOffset = distance * (height * 0.18);
                        final double scale = isSelected ? 1.0 : 0.85;
                        final double opacity = pow(0.6, distance.abs()).toDouble();
                        final double zIndex = -distance.abs().toDouble();
                        final bool isVisible = !_isEditing || isSelected;

                        final transform = Matrix4.identity()
                          ..setEntry(3, 2, 0.0015)
                          ..translate(0.0, verticalOffset)
                          ..scale(scale);

                        calculatedItems.add({
                          'widget': entry.buildEntry(context, isSelected, _isEditing && isSelected),
                          'transform': transform,
                          'opacity': isVisible ? opacity : 0.0,
                          'zIndex': zIndex,
                          'height': height,
                          'width': constraints.maxWidth * (isSelected ? 0.8 : 0.7),
                        });
                      }

                      calculatedItems.sort((a, b) => a['zIndex'].compareTo(b['zIndex']));

                      transformedItems.addAll(
                        calculatedItems.map((item) {
                          return AnimatedContainer(
                            duration: _animationDuration,
                            curve: _animationCurve,
                            transformAlignment: Alignment.center,
                            transform: item['transform'],
                            height: item['height'],
                            width: item['width'],
                            child: AnimatedOpacity(
                              duration: _animationDuration,
                              opacity: item['opacity'],
                              child: item['widget'],
                            ),
                          );
                        }),
                      );

                      return Stack(
                        alignment: Alignment.center,
                        children: [
                          ...transformedItems,
                          Positioned(
                            top: 0,
                            left: 0,
                            right: 0,
                            child: Container(
                              height: constraints.maxHeight * 0.2,
                              color: Theme.of(context).scaffoldBackgroundColor,
                              alignment: Alignment.center,
                              child: _SettingsTitle(title: "Paramètres"),
                            ),
                          ),
                        ],
                      );
                    },
                  ),
                ),
              ),
            ),
          ),
        ),
      ),
    );
  }
}

class _SettingsTitle extends StatelessWidget {
  final String title;
  const _SettingsTitle({required this.title});

  @override
  Widget build(BuildContext context) {
    return Text(
      title.toUpperCase(),
      style: TextStyle(
        color: Theme.of(context).primaryColor,
        fontSize: 16,
        fontWeight: FontWeight.bold,
        letterSpacing: 1.2,
      ),
    );
  }
}
