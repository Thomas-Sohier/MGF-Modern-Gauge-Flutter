import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:go_router/go_router.dart';
import 'package:modern_gauge_flutter/providers/settings_provider.dart';
import 'package:modern_gauge_flutter/routes/navigation_logic.dart';
import 'package:modern_gauge_flutter/ui/screens/settings/settings_apparence_pages.dart';
import 'package:modern_gauge_flutter/ui/screens/settings/settings_page_input.dart';
import 'package:modern_gauge_flutter/ui/themes/app_text_styles.dart';
import 'package:provider/provider.dart';
import 'package:modern_gauge_flutter/ui/screens/settings/settings_ecu_pages.dart';
import 'package:modern_gauge_flutter/ui/screens/settings/settings_ecrans_pages.dart';
import 'package:modern_gauge_flutter/ui/screens/settings/settings_systeme_pages.dart';
import 'package:modern_gauge_flutter/ui/widgets/circular_content.dart';
import 'package:modern_gauge_flutter/ui/widgets/settings_widgets.dart';
import 'package:modern_gauge_flutter/utils/no_traversal_policy.dart';

// ── Définition des catégories ──────────────────────────────────────────────

enum _Category {
  ecu,
  apparence,
  ecrans,
  systeme;

  String get label => switch (this) {
    _Category.ecu => 'ECU INFOS',
    _Category.apparence => 'APPARENCE',
    _Category.ecrans => 'ÉCRANS',
    _Category.systeme => 'SYSTÈME',
  };

  IconData get icon => switch (this) {
    _Category.ecu => Icons.memory_rounded,
    _Category.apparence => Icons.palette_outlined,
    _Category.ecrans => Icons.tv_rounded,
    _Category.systeme => Icons.tune_rounded,
  };

  List<SettingsPage> get pages => switch (this) {
    _Category.ecu => buildEcuPages(),
    _Category.apparence => buildApparencePages(),
    _Category.ecrans => buildEcransPages(),
    _Category.systeme => buildSystemePages(),
  };
}

// ── Écran principal ────────────────────────────────────────────────────────

class SettingsScreen extends StatefulWidget {
  const SettingsScreen({super.key});

  @override
  State<SettingsScreen> createState() => _SettingsScreenState();
}

class _SettingsScreenState extends State<SettingsScreen> {
  final _focusNode = FocusNode();
  late PageController _pageController;
  late List<SettingsPage> _pages;
  int _page = 0;
  bool _valueEditing = false; // focus capturé par un réglage « valeur »
  bool _toggleFlash = false; // flash bref après un toggle « simple »
  Timer? _flashTimer;
  int _rootPage = 0; // position mémorisée de la page racine
  _Category? _category;

  @override
  void initState() {
    super.initState();
    _pageController = PageController();
    _pages = _buildRootPages();
    WidgetsBinding.instance.addPostFrameCallback(
      (_) => _focusNode.requestFocus(),
    );
  }

  @override
  void dispose() {
    _flashTimer?.cancel();
    _focusNode.dispose();
    _pageController.dispose();
    super.dispose();
  }

  // ── Navigation ─────────────────────────────────────────────────────────

  List<SettingsPage> _buildRootPages() => _Category.values
      .map(
        (c) => SettingsPage(
          _CategoryCard(category: c, onTap: () => _enterCategory(c)),
          input: SimpleSettingsInput((_) => _enterCategory(c)),
        ),
      )
      .toList();

  void _enterCategory(_Category category) {
    _rootPage = _page; // mémorise la position avant d'entrer dans la catégorie
    setState(() {
      _category = category;
      _page = 0;
      _valueEditing = false;
      _pages = category.pages;
      _pageController.dispose();
      _pageController = PageController();
    });
  }

  void _backToRoot() {
    setState(() {
      _category = null;
      _page = _rootPage;
      _valueEditing = false;
      _pages = _buildRootPages();
      _pageController.dispose();
      _pageController = PageController(initialPage: _rootPage);
    });
  }

  void _exit() {
    final enabledScreens = context
        .read<SettingsProvider>()
        .settings
        .enabledScreens;
    context.go(buildDashboardRoutes(enabledScreens).first);
  }

  void _handleBack() {
    if (_category != null) {
      _backToRoot();
    } else {
      _exit();
    }
  }

  void _prevPage() {
    if (_page > 0) {
      _pageController.previousPage(
        duration: const Duration(milliseconds: 200),
        curve: Curves.easeInOut,
      );
    }
  }

  void _nextPage() {
    if (_page < _pages.length - 1) {
      _pageController.nextPage(
        duration: const Duration(milliseconds: 200),
        curve: Curves.easeInOut,
      );
    }
  }

  /// « OK » : délègue au [SettingsPageInput] de la page visible — toggle en
  /// mode simple, capture/libération du focus en mode valeur.
  void _handleOk() {
    switch (_pages[_page].input) {
      case SimpleSettingsInput(:final onToggle):
        onToggle(context);
        _flashCurrentCard();
      case ValueSettingsInput():
        setState(() => _valueEditing = !_valueEditing);
      case null:
        break;
    }
  }

  /// Flash bref de la carte visible pour confirmer un toggle « simple ».
  void _flashCurrentCard() {
    _flashTimer?.cancel();
    setState(() => _toggleFlash = true);
    _flashTimer = Timer(const Duration(milliseconds: 300), () {
      if (mounted) setState(() => _toggleFlash = false);
    });
  }

  void _handleKeyEvent(KeyEvent event) {
    if (event is! KeyDownEvent) return;
    final key = event.logicalKey;
    final isOk =
        key == LogicalKeyboardKey.enter || key == LogicalKeyboardKey.select;

    // Focus capturé : gauche/droite ajustent la valeur, OK/back libèrent.
    if (_valueEditing) {
      final input = _pages[_page].input;
      if (input is! ValueSettingsInput) {
        _valueEditing = false;
        return;
      }
      if (key == LogicalKeyboardKey.arrowLeft) {
        input.onDecrease(context);
      } else if (key == LogicalKeyboardKey.arrowRight) {
        input.onIncrease(context);
      } else if (isOk || key == LogicalKeyboardKey.escape) {
        setState(() => _valueEditing = false);
      }
      return;
    }

    if (key == LogicalKeyboardKey.arrowLeft) {
      _prevPage();
    } else if (key == LogicalKeyboardKey.arrowRight) {
      _nextPage();
    } else if (key == LogicalKeyboardKey.escape) {
      _handleBack();
    } else if (isOk) {
      _handleOk();
    }
  }

  String get _title => _category?.label ?? 'PARAMÈTRES';

  // ── Build ───────────────────────────────────────────────────────────────

  @override
  Widget build(BuildContext context) {
    return Material(
      color: Colors.transparent,
      child: CircularContent(
        child: ColoredBox(
          color: Theme.of(context).scaffoldBackgroundColor,
          child: KeyboardListener(
            focusNode: _focusNode,
            onKeyEvent: _handleKeyEvent,
            child: FocusTraversalGroup(
              policy: NoTraversalPolicy(),
              child: LayoutBuilder(
                builder: (context, constraints) {
                  final size = constraints.maxWidth;
                  return Stack(
                    alignment: Alignment.center,
                    children: [
                      Positioned(
                        top: size * 0.20,
                        bottom: 0,
                        left: 0,
                        right: 0,
                        child: AnimatedSwitcher(
                          duration: const Duration(milliseconds: 180),
                          transitionBuilder: (child, animation) =>
                              FadeTransition(opacity: animation, child: child),
                          child: KeyedSubtree(
                            key: ValueKey(_category),
                            child: _PagerBody(
                              pages: [
                                for (final (i, p) in _pages.indexed)
                                  (_valueEditing || _toggleFlash) && i == _page
                                      ? _EditingHighlight(child: p.widget)
                                      : p.widget,
                              ],
                              controller: _pageController,
                              currentIndex: _page,
                              onPageChanged: (i) => setState(() {
                                _page = i;
                                _valueEditing = false;
                              }),
                              onPrev: _page > 0 ? _prevPage : null,
                              onNext: _page < _pages.length - 1
                                  ? _nextPage
                                  : null,
                            ),
                          ),
                        ),
                      ),
                      Positioned(
                        top: 0,
                        left: 0,
                        right: 0,
                        child: SettingsHeader(
                          title: _title,
                          onBack: _handleBack,
                          height: size * 0.20,
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
    );
  }
}

// ── Surbrillance du mode édition ────────────────────────────────────────────

/// Retour visuel quand un [ValueSettingsInput] a capturé le focus : bordure
/// et fond légèrement teintés de la couleur primaire.
class _EditingHighlight extends StatelessWidget {
  final Widget child;

  const _EditingHighlight({required this.child});

  @override
  Widget build(BuildContext context) {
    final primary = Theme.of(context).colorScheme.primary;
    return DecoratedBox(
      decoration: BoxDecoration(
        color: primary.withValues(alpha: 0.08),
        border: Border.symmetric(
          horizontal: BorderSide(color: primary, width: 4),
        ),
      ),
      child: child,
    );
  }
}

// ── Pager body ──────────────────────────────────────────────────────────────

class _PagerBody extends StatelessWidget {
  final List<Widget> pages;
  final PageController controller;
  final int currentIndex;
  final void Function(int) onPageChanged;
  final VoidCallback? onPrev;
  final VoidCallback? onNext;

  const _PagerBody({
    required this.pages,
    required this.controller,
    required this.currentIndex,
    required this.onPageChanged,
    required this.onPrev,
    required this.onNext,
  });

  @override
  Widget build(BuildContext context) {
    return Column(
      children: [
        Expanded(
          child: PageView(
            controller: controller,
            onPageChanged: onPageChanged,
            children: pages,
          ),
        ),
        SettingsNavBar(
          index: currentIndex,
          total: pages.length,
          onPrev: onPrev,
          onNext: onNext,
          height: MediaQuery.of(context).size.height * 0.2,
        ),
      ],
    );
  }
}

// ── Carte de catégorie (niveau racine) ─────────────────────────────────────

class _CategoryCard extends StatelessWidget {
  final _Category category;
  final VoidCallback onTap;

  const _CategoryCard({required this.category, required this.onTap});

  @override
  Widget build(BuildContext context) {
    final primary = Theme.of(context).primaryColor;
    return Material(
      color: Colors.transparent,
      child: InkWell(
        onTap: onTap,
        child: SettingsCardShell(
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              Icon(category.icon, size: 40, color: primary),
              const SizedBox(height: 16),
              Text(
                category.label,
                style: AppTextStyles.body.copyWith(fontWeight: FontWeight.bold),
              ),
              const SizedBox(height: 34),
              Icon(Icons.arrow_circle_right, size: 28, color: primary),
            ],
          ),
        ),
      ),
    );
  }
}
