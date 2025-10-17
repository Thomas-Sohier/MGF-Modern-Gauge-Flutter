import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/app.dart';
import 'package:modern_gauge_flutter/providers/app_state_provider.dart';
import 'package:modern_gauge_flutter/providers/dial_provider.dart';
import 'package:modern_gauge_flutter/providers/settings_provider.dart';
import 'package:modern_gauge_flutter/routes/app_router.dart';
import 'package:modern_gauge_flutter/services/mpris_listener.dart';
import 'package:modern_gauge_flutter/services/odb_service.dart';
import 'package:modern_gauge_flutter/services/settings_service.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await SettingsService().init();
  final odbService = OdbService();
  final settingsProvider = SettingsProvider();
  final mprisListener = MprisListener();
  final dialProvider = DialProvider(odbService);
  final appStateProvider = AppStateProvider(odbService);
  final router = AppRouter.router;

  runApp(
    App(
      odbService: odbService,
      settingsProvider: settingsProvider,
      dialProvider: dialProvider,
      mprisListener: mprisListener,
      appStateProvider: appStateProvider,
      router: router,
    ),
  );
}
