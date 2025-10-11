import 'package:go_router/go_router.dart';
import 'package:flutter/material.dart'; // Nécessaire pour les Widgets et BuildContext

import 'package:modern_gauge_flutter/routes/route_names.dart';
import 'package:modern_gauge_flutter/ui/screens/clock_screen.dart';
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
        builder: (BuildContext context, GoRouterState state) => const SplashScreen(),
      ),

      ShellRoute(
        builder: (BuildContext context, GoRouterState state, Widget child) {
          return DashboardShellScreen(child: child);
        },
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
        ],
      ),

      GoRoute(
        path: RouteNames.settingsRoute,
        builder: (BuildContext context, GoRouterState state) => const SettingsScreen(),
      ),
    ],
    errorBuilder: (context, state) => const Scaffold(body: Center(child: Text('Error: Page not found'))),
  );
}
