import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';
import 'package:modern_gauge_flutter/providers/ecu_provider.dart';
import 'package:modern_gauge_flutter/providers/app_state_provider.dart';
import 'package:modern_gauge_flutter/providers/mpris_provider.dart';
import 'package:modern_gauge_flutter/providers/settings_provider.dart';
import 'package:modern_gauge_flutter/services/head_unit_control_service.dart';
import 'package:modern_gauge_flutter/services/navigation_server_listener.dart';
import 'package:modern_gauge_flutter/services/notification_server_listener.dart';
import 'package:modern_gauge_flutter/ui/themes/app_theme.dart';
import 'package:modern_gauge_flutter/ui/widgets/notification_overlay.dart';
import 'package:provider/provider.dart';

class App extends StatelessWidget {
  final SettingsProvider settingsProvider;
  final MprisListenerBase mprisListener;
  final NavigationServerListener navigationListener;
  final NotificationServerListener notificationListener;
  final AppStateProvider appStateProvider;
  final EcuProvider ecuProvider;
  final HeadUnitControlService headUnitControlService;
  final GoRouter router;

  const App({
    super.key,
    required this.settingsProvider,
    required this.mprisListener,
    required this.navigationListener,
    required this.notificationListener,
    required this.appStateProvider,
    required this.ecuProvider,
    required this.headUnitControlService,
    required this.router,
  });

  @override
  Widget build(BuildContext context) {
    return MultiProvider(
      providers: [
        ChangeNotifierProvider(create: (_) => settingsProvider),
        ChangeNotifierProvider<MprisListenerBase>(
          lazy: false,
          create: (context) => mprisListener..start(),
        ),
        ChangeNotifierProvider<NavigationServerListener>(
          lazy: false,
          create: (context) => navigationListener..start(),
        ),
        Provider<NotificationServerListener>(
          lazy: false,
          create: (context) => notificationListener..start(),
          dispose: (context, listener) => listener.dispose(),
        ),
        Provider<HeadUnitControlService>(
          lazy: false,
          create: (context) => headUnitControlService..start(),
          dispose: (context, service) => service.dispose(),
        ),
        ChangeNotifierProvider(create: (context) => appStateProvider),
        ChangeNotifierProvider(create: (context) => ecuProvider),
      ],
      child: Selector<SettingsProvider, ThemeMode>(
        selector: (_, listener) => listener.settings.themeMode,
        builder: (context, status, _) {
          return MaterialApp.router(
            title: 'ODB Dashboard',
            theme: AppTheme.lightTheme,
            darkTheme: AppTheme.darkTheme,
            themeMode: status,
            routerConfig: router,
            debugShowCheckedModeBanner: false,
            builder: (context, child) => NotificationOverlayHost(
              router: router,
              child: child ?? const SizedBox.shrink(),
            ),
          );
        },
      ),
    );
  }
}
