import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';
import 'package:modern_gauge_flutter/mixins/screen_navigation_mixin.dart';
import 'package:modern_gauge_flutter/providers/ecu_provider.dart';
import 'package:modern_gauge_flutter/routes/navigation_logic.dart';
import 'package:modern_gauge_flutter/routes/route_names.dart';
import 'package:modern_gauge_flutter/ui/themes/app_text_styles.dart';
import 'package:provider/provider.dart';

class FaultsScreen extends StatefulWidget {
  const FaultsScreen({super.key});

  @override
  State<FaultsScreen> createState() => _FaultsScreenState();
}

class _FaultsScreenState extends State<FaultsScreen>
    with ScreenNavigationMixin<FaultsScreen> {
  @override
  void nextScreen() {
    const currentRoute = RouteNames.dashboardRoute + RouteNames.faultsRoute;
    context.go(getNextRoute(currentRoute, enabledScreens));
  }

  @override
  void previousScreen() {
    const currentRoute = RouteNames.dashboardRoute + RouteNames.faultsRoute;
    context.go(getPreviousRoute(currentRoute, enabledScreens));
  }

  @override
  Widget build(BuildContext context) {
    return buildNavigableScreen(child: const _FaultsContent());
  }
}

class _FaultsContent extends StatelessWidget {
  const _FaultsContent();

  @override
  Widget build(BuildContext context) {
    return Selector<EcuProvider, List<dynamic>?>(
      selector: (_, provider) => provider.currentData.faults,
      builder: (context, faults, _) {
        final hasFaults = faults != null && faults.isNotEmpty;

        return Padding(
          padding: const EdgeInsets.symmetric(horizontal: 32, vertical: 24),
          child: Column(
            children: [
              const _FaultsHeader(),
              const SizedBox(height: 12),
              Expanded(
                child: hasFaults
                    ? _FaultsList(faults: faults)
                    : const _NoFaultsMessage(),
              ),
              if (hasFaults) const SizedBox(height: 12),
              if (hasFaults) const _ClearFaultsButton(),
            ],
          ),
        );
      },
    );
  }
}

class _FaultsHeader extends StatelessWidget {
  const _FaultsHeader();

  @override
  Widget build(BuildContext context) {
    return const Text('Codes erreurs', style: AppTextStyles.title);
  }
}

class _NoFaultsMessage extends StatelessWidget {
  const _NoFaultsMessage();

  @override
  Widget build(BuildContext context) {
    return Row(
      mainAxisAlignment: MainAxisAlignment.center,
      children: [
        const Icon(Icons.check_circle_outline, size: 24),
        const SizedBox(width: 8),
        const Text('Aucun code erreur', style: AppTextStyles.body),
      ],
    );
  }
}

class _FaultsList extends StatelessWidget {
  final List<dynamic> faults;

  const _FaultsList({required this.faults});

  @override
  Widget build(BuildContext context) {
    return ListView.separated(
      itemCount: faults.length,
      separatorBuilder: (_, __) => const SizedBox(height: 6),
      itemBuilder: (context, index) =>
          _FaultItem(label: faults[index].toString()),
    );
  }
}

class _FaultItem extends StatelessWidget {
  final String label;

  const _FaultItem({required this.label});

  @override
  Widget build(BuildContext context) {
    return Row(
      children: [
        Icon(
          Icons.warning_amber_rounded,
          size: 16,
          color: Theme.of(context).colorScheme.error,
        ),
        const SizedBox(width: 8),
        Expanded(
          child: Text(label, style: AppTextStyles.small),
        ),
      ],
    );
  }
}

class _ClearFaultsButton extends StatelessWidget {
  const _ClearFaultsButton();

  @override
  Widget build(BuildContext context) {
    return TextButton.icon(
      onPressed: () => context.read<EcuProvider>().clearFaults(),
      icon: const Icon(Icons.delete_sweep_outlined, size: 18),
      label: const Text('Clear'),
    );
  }
}
