import 'package:go_router/go_router.dart';
import 'package:flutter/material.dart'; // Nécessaire pour les Widgets et BuildContext

import 'package:modern_gauge_flutter/routes/route_names.dart';
import 'package:modern_gauge_flutter/ui/screens/admission_screen.dart';
import 'package:modern_gauge_flutter/ui/screens/allumage_screen.dart';
import 'package:modern_gauge_flutter/ui/screens/clock_screen.dart';
import 'package:modern_gauge_flutter/ui/screens/faults_screen.dart';
import 'package:modern_gauge_flutter/ui/screens/injection_screen.dart';
import 'package:modern_gauge_flutter/ui/screens/lambda_screen.dart';
import 'package:modern_gauge_flutter/ui/screens/ralenti_screen.dart';
import 'package:modern_gauge_flutter/ui/screens/rpm_screen.dart';
import 'package:modern_gauge_flutter/ui/screens/dashboard_shell_screen.dart';
import 'package:modern_gauge_flutter/ui/screens/music_player_screen.dart';
import 'package:modern_gauge_flutter/ui/screens/settings_screen.dart';
import 'package:modern_gauge_flutter/ui/screens/splash_screen.dart';
import 'package:modern_gauge_flutter/ui/screens/temps_screen.dart';

class AppRouter {
  static final GoRouter router = GoRouter(
    initialLocation: RouteNames.splashRoute,
    routes: <RouteBase>[
      GoRoute(
        path: RouteNames.splashRoute,
        pageBuilder: (context, state) => const NoTransitionPage(child: SplashScreen()),
      ),

      ShellRoute(
        pageBuilder: (context, state, child) => NoTransitionPage(child: DashboardShellScreen(child: child)),
        routes: <RouteBase>[
          GoRoute(
            path: RouteNames.timeFull,
            pageBuilder: (context, state) => const NoTransitionPage(child: ClockScreen()),
          ),
          GoRoute(
            path: RouteNames.rpmFull,
            pageBuilder: (context, state) => const NoTransitionPage(child: RpmScreen()),
          ),
          GoRoute(
            path: RouteNames.musicFull,
            pageBuilder: (context, state) => const NoTransitionPage(child: MusicPlayerScreen()),
          ),
          GoRoute(
            path: RouteNames.faultsFull,
            pageBuilder: (context, state) => const NoTransitionPage(child: FaultsScreen()),
          ),
          GoRoute(
            path: RouteNames.tempsFull,
            pageBuilder: (context, state) => const NoTransitionPage(child: TempsScreen()),
          ),
          GoRoute(
            path: RouteNames.injectionFull,
            pageBuilder: (context, state) => const NoTransitionPage(child: InjectionScreen()),
          ),
          GoRoute(
            path: RouteNames.lambdaFull,
            pageBuilder: (context, state) => const NoTransitionPage(child: LambdaScreen()),
          ),
          GoRoute(
            path: RouteNames.allumageFull,
            pageBuilder: (context, state) => const NoTransitionPage(child: AllumageScreen()),
          ),
          GoRoute(
            path: RouteNames.ralentiFull,
            pageBuilder: (context, state) => const NoTransitionPage(child: RalentiScreen()),
          ),
          GoRoute(
            path: RouteNames.admissionFull,
            pageBuilder: (context, state) => const NoTransitionPage(child: AdmissionScreen()),
          ),
        ],
      ),

      GoRoute(
        path: RouteNames.settingsRoute,
        pageBuilder: (context, state) => const NoTransitionPage(child: SettingsScreen()),
      ),
    ],
    errorBuilder: (context, state) => const Scaffold(body: Center(child: Text('Error: Page not found'))),
  );
}
