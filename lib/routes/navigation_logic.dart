// lib/routes/navigation_logic.dart

import 'package:modern_gauge_flutter/routes/route_names.dart';

/// Ordre fixe de tous les écrans du tableau de bord.
const List<String> allDashboardRoutes = [
  RouteNames.timeFull,
  RouteNames.musicFull,
  RouteNames.rpmFull,
  RouteNames.faultsFull,
  RouteNames.tempsFull,
  RouteNames.injectionFull,
  RouteNames.lambdaFull,
  RouteNames.allumageFull,
  RouteNames.ralentiFull,
  RouteNames.admissionFull,
];

/// Filtre la liste complète selon les segments activés (ex: {'/rpm', '/time'}).
/// Si aucun écran n'est actif, retourne l'horloge comme fallback.
List<String> buildDashboardRoutes(Set<String> enabledScreens) {
  final routes = allDashboardRoutes
      .where((r) => enabledScreens.any((seg) => r.endsWith(seg)))
      .toList();
  return routes.isEmpty ? [RouteNames.timeFull] : routes;
}

/// Retourne la route suivante dans la séquence cyclique.
String getNextRoute(String currentRoute, Set<String> enabledScreens) {
  final routes = buildDashboardRoutes(enabledScreens);
  if (routes.isEmpty) return currentRoute;
  final currentIndex = routes.indexOf(currentRoute);
  if (currentIndex == -1) return routes.first;
  return routes[(currentIndex + 1) % routes.length];
}

/// Retourne la route précédente dans la séquence cyclique.
String getPreviousRoute(String currentRoute, Set<String> enabledScreens) {
  final routes = buildDashboardRoutes(enabledScreens);
  if (routes.isEmpty) return currentRoute;
  final currentIndex = routes.indexOf(currentRoute);
  if (currentIndex == -1) return routes.first;
  return routes[(currentIndex - 1 + routes.length) % routes.length];
}
