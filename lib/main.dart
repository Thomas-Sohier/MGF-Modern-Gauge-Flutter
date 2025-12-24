import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/app.dart';
import 'package:modern_gauge_flutter/providers/app_state_provider.dart';
import 'package:modern_gauge_flutter/providers/dial_provider.dart';
import 'package:modern_gauge_flutter/providers/ecu_provider.dart';
import 'package:modern_gauge_flutter/providers/settings_provider.dart';
import 'package:modern_gauge_flutter/routes/app_router.dart';
import 'package:modern_gauge_flutter/services/ecu_service.dart';
import 'package:modern_gauge_flutter/services/log_service.dart';
import 'package:modern_gauge_flutter/services/mpris_listener.dart';
import 'package:modern_gauge_flutter/services/odb_service.dart';
import 'package:modern_gauge_flutter/services/settings_service.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await LogService.initialize();
  await SettingsService().init();
  final ecuService = EcuService();
  final odbService = OdbService(ecuService);
  final settingsProvider = SettingsProvider();
  final mprisListener = MprisListener();
  final dialProvider = DialProvider(odbService);
  final appStateProvider = AppStateProvider(odbService);
  final ecuProvider = EcuProvider(ecuService);
  final router = AppRouter.router;

  LogService.info("Application startup.");
  runApp(
    App(
      odbService: odbService,
      settingsProvider: settingsProvider,
      dialProvider: dialProvider,
      mprisListener: mprisListener,
      appStateProvider: appStateProvider,
      ecuProvider: ecuProvider,
      router: router,
    ),
  );
}
