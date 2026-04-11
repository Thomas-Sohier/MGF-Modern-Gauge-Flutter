// lib/routes/navigation_logic.dart

import 'package:modern_gauge_flutter/routes/route_names.dart';

/// La liste ordonnée des routes du tableau de bord pour la navigation cyclique.
final List<String> dashboardRoutes = [
  RouteNames.dashboardRoute + RouteNames.rpmRoute,
  // RouteNames.dashboardRoute + RouteNames.musicRoute,
  RouteNames.dashboardRoute + RouteNames.timeRoute,
  RouteNames.dashboardRoute + RouteNames.faultsRoute,
];

/// Retourne la route suivante dans la séquence cyclique.
String getNextRoute(String currentRoute) {
  final currentIndex = dashboardRoutes.indexOf(currentRoute);
  // Si la route n'est pas trouvée, on retourne à la première par sécurité.
  if (currentIndex == -1) return dashboardRoutes.first;

  // L'opérateur modulo (%) permet de revenir au début après la dernière route.
  final nextIndex = (currentIndex + 1) % dashboardRoutes.length;
  return dashboardRoutes[nextIndex];
}

/// Retourne la route précédente dans la séquence cyclique.
String getPreviousRoute(String currentRoute) {
  final currentIndex = dashboardRoutes.indexOf(currentRoute);
  if (currentIndex == -1) return dashboardRoutes.first;

  // Une astuce avec le modulo pour gérer l'index -1.
  final previousIndex = (currentIndex - 1 + dashboardRoutes.length) % dashboardRoutes.length;
  return dashboardRoutes[previousIndex];
}
