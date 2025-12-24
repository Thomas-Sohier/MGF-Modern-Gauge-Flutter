import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';
import 'package:modern_gauge_flutter/providers/ecu_provider.dart';
import 'package:modern_gauge_flutter/providers/app_state_provider.dart';
import 'package:modern_gauge_flutter/providers/dial_provider.dart';
import 'package:modern_gauge_flutter/providers/mpris_provider.dart';
import 'package:modern_gauge_flutter/providers/settings_provider.dart';
import 'package:modern_gauge_flutter/services/odb_service.dart';
import 'package:modern_gauge_flutter/ui/themes/app_theme.dart';
import 'package:provider/provider.dart';

class App extends StatelessWidget {
  final OdbService odbService;
  final SettingsProvider settingsProvider;
  final DialProvider dialProvider;
  final MprisListenerBase mprisListener;
  final AppStateProvider appStateProvider;
  final EcuProvider ecuProvider;
  final GoRouter router;

  const App({
    super.key,
    required this.odbService,
    required this.settingsProvider,
    required this.dialProvider,
    required this.mprisListener,
    required this.appStateProvider,
    required this.ecuProvider,
    required this.router,
  });

  @override
  Widget build(BuildContext context) {
    return MultiProvider(
      providers: [
        Provider(create: (_) => odbService, dispose: (_, service) => service.dispose()),
        ChangeNotifierProvider(create: (_) => settingsProvider),
        ChangeNotifierProvider(create: (context) => dialProvider),
        ChangeNotifierProvider<MprisListenerBase>(create: (context) => mprisListener..start()),
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
            themeMode: Provider.of<SettingsProvider>(context, listen: true).settings.themeMode,
            routerConfig: router,
            debugShowCheckedModeBanner: false,
          );
        },
      ),
    );
  }
}
