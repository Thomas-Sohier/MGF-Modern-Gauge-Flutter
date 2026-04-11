// settings_screen.dart

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
  final _pageFocusNode = FocusNode();

  late final List<FocusNode> _focusNodes;
  late final List<SettingsAbstractEntry> _entries;

  int _selectedIndex = 0;

  @override
  void initState() {
    super.initState();
    _focusNodes = [];
    _entries = [
      // SettingBrightnessEntry(focusNode: _newFocusNode()),
      SettingThemeEntry(focusNode: _newFocusNode()),
      SettingEcoEntry(focusNode: _newFocusNode()),
    ];

    WidgetsBinding.instance.addPostFrameCallback((_) {
      _pageFocusNode.requestFocus();
    });
  }

  @override
  void dispose() {
    _pageFocusNode.dispose();
    for (final node in _focusNodes) {
      node.dispose();
    }
    super.dispose();
  }

  FocusNode _newFocusNode() {
    final node = FocusNode(skipTraversal: true);
    _focusNodes.add(node);
    return node;
  }

  void _navigateUp() {
    if (_selectedIndex > 0) {
      setState(() => _selectedIndex--);
    }
  }

  void _navigateDown() {
    if (_selectedIndex < _entries.length - 1) {
      setState(() => _selectedIndex++);
    }
  }

  void _goBack() {
    _saveSettings();
    if (Navigator.canPop(context)) Navigator.pop(context);
    context.go(RouteNames.dashboardRoute + RouteNames.rpmRoute);
  }

  void _saveSettings() {
    final settingsProvider = Provider.of<SettingsProvider>(
      context,
      listen: false,
    );
    SettingsService().saveSettings(settingsProvider.settings);
  }

  void _handleKeyEvent(KeyEvent event) {
    if (event is! KeyDownEvent) return;
    if (event.logicalKey == LogicalKeyboardKey.arrowDown) {
      _navigateDown();
    } else if (event.logicalKey == LogicalKeyboardKey.arrowUp) {
      _navigateUp();
    } else if (event.logicalKey == LogicalKeyboardKey.escape) {
      _goBack();
    }
  }

  @override
  Widget build(BuildContext context) {
    return Material(
      color: Colors.transparent,
      child: Center(
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
                      final size = constraints.maxWidth;

                      return Stack(
                        alignment: Alignment.center,
                        children: [
                          // ── Item central : occupe toute la zone ──
                          Positioned(
                            top: size * 0.18, // sous le header
                            bottom: size * 0.20, // au-dessus des boutons
                            left: size * 0.1,
                            right: size * 0.1,
                            child: _buildCurrentEntry(context),
                          ),

                          // ── En-tête collé en haut ────────────────
                          Positioned(
                            top: size * 0.07,
                            left: size * 0.1,
                            right: size * 0.1,
                            child: _Header(onBack: _goBack),
                          ),

                          // ── Boutons + dots collés en bas ─────────
                          Positioned(
                            bottom: size * 0.03,
                            left: size * 0.1,
                            right: size * 0.1,
                            child: Column(
                              mainAxisSize: MainAxisSize.min,
                              children: [
                                Row(
                                  mainAxisAlignment: MainAxisAlignment.center,
                                  children: [
                                    _NavButton(
                                      icon: Icons.keyboard_arrow_up_rounded,
                                      enabled: _selectedIndex > 0,
                                      onTap: _navigateUp,
                                    ),
                                    const SizedBox(width: 12),
                                    _NavButton(
                                      icon: Icons.keyboard_arrow_down_rounded,
                                      enabled:
                                          _selectedIndex < _entries.length - 1,
                                      onTap: _navigateDown,
                                    ),
                                  ],
                                ),
                              ],
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

  Widget _buildCurrentEntry(BuildContext context) {
    return AnimatedSwitcher(
      duration: const Duration(milliseconds: 200),
      transitionBuilder: (child, animation) =>
          FadeTransition(opacity: animation, child: child),
      child: KeyedSubtree(
        key: ValueKey(_selectedIndex),
        child: _entries[_selectedIndex].buildEntry(context, true),
      ),
    );
  }
}

// ── Widgets internes ─────────────────────────────────────────────────────────

class _Header extends StatelessWidget {
  final VoidCallback onBack;
  const _Header({required this.onBack});

  @override
  Widget build(BuildContext context) {
    final color = Theme.of(context).primaryColor;
    return Row(
      mainAxisAlignment: MainAxisAlignment.center,
      children: [
        GestureDetector(
          onTap: onBack,
          child: Icon(Icons.arrow_back_ios_new_rounded, color: color, size: 24),
        ),
        const SizedBox(width: 8),
        Text(
          'PARAMÈTRES',
          style: TextStyle(
            color: color,
            fontSize: 20,
            fontWeight: FontWeight.bold,
            letterSpacing: 1.5,
          ),
        ),
      ],
    );
  }
}

class _NavButton extends StatelessWidget {
  final IconData icon;
  final bool enabled;
  final VoidCallback onTap;

  const _NavButton({
    required this.icon,
    required this.enabled,
    required this.onTap,
  });

  @override
  Widget build(BuildContext context) {
    final color = Theme.of(context).primaryColor;
    return GestureDetector(
      behavior: HitTestBehavior.opaque,
      onTap: enabled ? onTap : null,
      child: Center(
        child: AnimatedOpacity(
          duration: const Duration(milliseconds: 150),
          opacity: enabled ? 1.0 : 0.2,
          child: Icon(icon, color: color, size: 42),
        ),
      ),
    );
  }
}

class _PageDots extends StatelessWidget {
  final int count;
  final int current;
  const _PageDots({required this.count, required this.current});

  @override
  Widget build(BuildContext context) {
    final color = Theme.of(context).primaryColor;
    return Row(
      mainAxisAlignment: MainAxisAlignment.center,
      children: List.generate(count, (i) {
        return AnimatedContainer(
          duration: const Duration(milliseconds: 200),
          margin: const EdgeInsets.symmetric(horizontal: 3),
          width: i == current ? 10 : 6,
          height: i == current ? 10 : 6,
          decoration: BoxDecoration(
            color: i == current ? color : color.withOpacity(0.3),
            shape: BoxShape.circle,
          ),
        );
      }),
    );
  }
}
