import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';
import 'package:modern_gauge_flutter/providers/app_state_provider.dart';
import 'package:modern_gauge_flutter/providers/settings_provider.dart'; // Pour le mode de réveil
import 'package:modern_gauge_flutter/routes/navigation_logic.dart';
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
    if (!mounted) return;

    final appState = Provider.of<AppStateProvider>(context, listen: false);
    appState.finishInitialization();

    if (!appState.isAsleep) {
      final enabledScreens = Provider.of<SettingsProvider>(
        context,
        listen: false,
      ).settings.enabledScreens;
      GoRouter.of(context).go(buildDashboardRoutes(enabledScreens).first);
    }
  }

  @override
  Widget build(BuildContext context) {
    final appState = Provider.of<AppStateProvider>(context);
    if (!appState.isInitializing && !appState.isAsleep) {
      return const SizedBox.shrink();
    }
    return Scaffold(
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
    );
  }
}
