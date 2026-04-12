import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:go_router/go_router.dart';
import 'package:modern_gauge_flutter/routes/route_names.dart';
import 'package:modern_gauge_flutter/ui/screens/settings/settings_apparence_pages.dart';
import 'package:modern_gauge_flutter/ui/screens/settings/settings_ecu_pages.dart';
import 'package:modern_gauge_flutter/ui/screens/settings/settings_systeme_pages.dart';
import 'package:modern_gauge_flutter/ui/screens/settings/settings_widgets.dart';
import 'package:modern_gauge_flutter/utils/no_traversal_policy.dart';

// ── Définition des catégories ──────────────────────────────────────────────

enum _Category {
  ecu,
  apparence,
  systeme;

  String get label => switch (this) {
    _Category.ecu => 'ECU INFOS',
    _Category.apparence => 'APPARENCE',
    _Category.systeme => 'SYSTÈME',
  };

  IconData get icon => switch (this) {
    _Category.ecu => Icons.memory_rounded,
    _Category.apparence => Icons.palette_outlined,
    _Category.systeme => Icons.tune_rounded,
  };

  List<Widget> get pages => switch (this) {
    _Category.ecu => buildEcuPages(),
    _Category.apparence => buildApparencePages(),
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
  late List<Widget> _pages;
  int _page = 0;
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
    _focusNode.dispose();
    _pageController.dispose();
    super.dispose();
  }

  // ── Navigation ─────────────────────────────────────────────────────────

  List<Widget> _buildRootPages() => _Category.values
      .map((c) => _CategoryCard(category: c, onTap: () => _enterCategory(c)))
      .toList();

  void _enterCategory(_Category category) {
    _pageController.dispose();
    _pageController = PageController();
    setState(() {
      _category = category;
      _page = 0;
      _pages = category.pages;
    });
  }

  void _backToRoot() {
    _pageController.dispose();
    _pageController = PageController();
    setState(() {
      _category = null;
      _page = 0;
      _pages = _buildRootPages();
    });
  }

  void _exit() => context.go(RouteNames.dashboardRoute + RouteNames.rpmRoute);

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
    } else {
      _handleBack();
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

  void _handleKeyEvent(KeyEvent event) {
    if (event is! KeyDownEvent) return;
    final key = event.logicalKey;
    if (key == LogicalKeyboardKey.arrowLeft ||
        key == LogicalKeyboardKey.escape) {
      _prevPage();
    } else if (key == LogicalKeyboardKey.arrowRight) {
      _nextPage();
    }
  }

  String get _title => _category?.label ?? 'PARAMÈTRES';

  // ── Build ───────────────────────────────────────────────────────────────

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
                                  FadeTransition(
                                    opacity: animation,
                                    child: child,
                                  ),
                              child: KeyedSubtree(
                                key: ValueKey(_category),
                                child: _PagerBody(
                                  pages: _pages,
                                  controller: _pageController,
                                  currentIndex: _page,
                                  onPageChanged: (i) =>
                                      setState(() => _page = i),
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
        ),
      ),
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
                style: TextStyle(
                  fontFamily: 'JetBrainsMono',
                  fontSize: 16,
                  fontWeight: FontWeight.w700,
                  color: primary,
                  letterSpacing: 1.2,
                ),
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
