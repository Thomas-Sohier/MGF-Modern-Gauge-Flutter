import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';
import 'package:modern_gauge_flutter/models/settings_data.dart'; // Pour l'enum WakeUpMode
import 'package:modern_gauge_flutter/providers/app_state_provider.dart';
import 'package:modern_gauge_flutter/providers/settings_provider.dart'; // Pour le mode de réveil
import 'package:modern_gauge_flutter/routes/route_names.dart';
import 'package:provider/provider.dart';

class SplashScreen extends StatefulWidget {
  const SplashScreen({super.key});

  @override
  State<SplashScreen> createState() => _SplashScreenState();
}

class _SplashScreenState extends State<SplashScreen> {
  @override
  void initState() {
    super.initState();
    _initializeApp();
  }

  Future<void> _initializeApp() async {
    await Future.delayed(const Duration(seconds: 2));
    final appState = Provider.of<AppStateProvider>(context, listen: false);
    appState.finishInitialization();

    if (!appState.isAsleep) {
      if (mounted) {
        GoRouter.of(
          context,
        ).go(RouteNames.dashboardRoute + RouteNames.rpmRoute);
      }
    }
  }

  // Gère la détection de l'appui pour sortir du mode veille
  void _handleScreenTap() {
    final appState = Provider.of<AppStateProvider>(context, listen: false);
    final settings = Provider.of<SettingsProvider>(
      context,
      listen: false,
    ).settings;

    if (appState.isAsleep && settings.wakeUpMode == WakeUpMode.longPress) {
      // Pour simuler un appui long, on peut utiliser un GestureDetector avec onLongPress
      // Ou si c'est un simple tap pour sortir, alors on l'active ici
      // Pour l'instant, faisons-le réagir à un tap pour tester.
      // Dans une implémentation réelle, tu auras besoin d'un onLongPress ou d'un bouton physique.
      // TODO
      _wakeUpApp();
    } else if (appState.isAsleep && settings.wakeUpMode == WakeUpMode.onStart) {
      _wakeUpApp(); // Si le mode est "onStart", un tap peut réveiller aussi
    }
  }

  void _wakeUpApp() {
    final appState = Provider.of<AppStateProvider>(context, listen: false);
    appState.setSleepMode(false);
    if (mounted) {
      GoRouter.of(context).go(RouteNames.dashboardRoute);
    }
  }

  @override
  Widget build(BuildContext context) {
    final appState = Provider.of<AppStateProvider>(context);
    if (!appState.isInitializing && !appState.isAsleep) {
      return const SizedBox.shrink();
    }
    return GestureDetector(
      onTap: _handleScreenTap,
      child: Scaffold(
        backgroundColor: Colors.black,
        body: Center(
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              Image.asset('assets/images/mg_logo.png'),
              const SizedBox(height: 15),
              if (appState.isAsleep)
                const Text(
                  'Appuyez pour réveiller',
                  style: TextStyle(color: Colors.white70, fontSize: 16),
                ),
            ],
          ),
        ),
      ),
    );
  }
}
