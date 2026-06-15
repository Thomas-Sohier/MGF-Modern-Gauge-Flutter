import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter/scheduler.dart';
import 'package:modern_gauge_flutter/app.dart';
import 'package:modern_gauge_flutter/providers/app_state_provider.dart';
import 'package:modern_gauge_flutter/providers/ecu_provider.dart';
import 'package:modern_gauge_flutter/providers/settings_provider.dart';
import 'package:modern_gauge_flutter/routes/app_router.dart';
import 'package:modern_gauge_flutter/services/ecu_service.dart';
import 'package:modern_gauge_flutter/services/log_service.dart';
import 'package:modern_gauge_flutter/services/navigation_server_listener.dart';
import 'package:modern_gauge_flutter/services/notification_server_listener.dart';
import 'package:modern_gauge_flutter/services/now_playing_server_listener.dart';
import 'package:modern_gauge_flutter/services/settings_service.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await LogService.initialize();
  await SettingsService().init();
  final ecuService = EcuService();
  final settingsProvider = SettingsProvider();
  final mprisListener = NowPlayingServerListener();
  final navigationListener = NavigationServerListener();
  final notificationListener = NotificationServerListener();
  final appStateProvider = AppStateProvider();
  final ecuProvider = EcuProvider(ecuService);
  final router = AppRouter.router;

  LogService.info("Application startup.");

  // Re-apply the persisted WiFi choice on boot. rfkill state does not survive a
  // reboot on this image, so if WiFi was turned off in settings, push that to
  // the device again (best-effort; the Go server starts before the UI).
  if (!settingsProvider.settings.wifiEnabled) {
    ecuProvider.setWifiEnabled(false);
  }

  SchedulerBinding.instance.addPostFrameCallback((_) async {
    if (!Platform.isLinux) return;
    try {
      await Process.run('psplash-write', const ['QUIT']);
    } catch (_) {
      // psplash not running / not present — ignore
    }
  });
  runApp(
    App(
      settingsProvider: settingsProvider,
      mprisListener: mprisListener,
      navigationListener: navigationListener,
      notificationListener: notificationListener,
      appStateProvider: appStateProvider,
      ecuProvider: ecuProvider,
      router: router,
    ),
  );
}
