import 'package:go_router/go_router.dart';
import 'package:flutter/material.dart'; // Nécessaire pour les Widgets et BuildContext

import 'package:modern_gauge_flutter/routes/route_names.dart';
import 'package:modern_gauge_flutter/ui/screens/clock_screen.dart';
import 'package:modern_gauge_flutter/ui/screens/faults_screen.dart';
import 'package:modern_gauge_flutter/ui/screens/rpm_screen.dart';
import 'package:modern_gauge_flutter/ui/screens/dashboard_shell_screen.dart';
import 'package:modern_gauge_flutter/ui/screens/music_player_screen.dart';
import 'package:modern_gauge_flutter/ui/screens/settings_screen.dart';
import 'package:modern_gauge_flutter/ui/screens/splash_screen.dart';

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
            path: RouteNames.dashboardRoute + RouteNames.timeRoute,
            pageBuilder: (context, state) => const NoTransitionPage(child: ClockScreen()),
          ),
          GoRoute(
            path: RouteNames.dashboardRoute + RouteNames.rpmRoute,
            pageBuilder: (context, state) => const NoTransitionPage(child: RpmScreen()),
          ),
          GoRoute(
            path: RouteNames.dashboardRoute + RouteNames.musicRoute,
            pageBuilder: (context, state) => const NoTransitionPage(child: MusicPlayerScreen()),
          ),
          GoRoute(
            path: RouteNames.dashboardRoute + RouteNames.faultsRoute,
            pageBuilder: (context, state) => const NoTransitionPage(child: FaultsScreen()),
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
