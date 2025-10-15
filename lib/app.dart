import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/providers/app_state_provider.dart';
import 'package:modern_gauge_flutter/providers/dial_provider.dart';
import 'package:modern_gauge_flutter/providers/mpris_provider.dart';
import 'package:modern_gauge_flutter/providers/settings_provider.dart';
import 'package:modern_gauge_flutter/routes/app_router.dart';
import 'package:modern_gauge_flutter/services/mpris_listener.dart';
import 'package:modern_gauge_flutter/services/odb_service.dart';
import 'package:modern_gauge_flutter/ui/themes/app_theme.dart';
import 'package:provider/provider.dart';

class App extends StatelessWidget {
  const App({super.key});

  @override
  Widget build(BuildContext context) {
    return MultiProvider(
      providers: [
        Provider(create: (_) => OdbService(), dispose: (_, service) => service.dispose()),
        ChangeNotifierProvider(create: (_) => SettingsProvider()),
        ChangeNotifierProvider(create: (context) => DialProvider(context.read<OdbService>())),
        ChangeNotifierProvider<MprisListenerBase>(create: (context) => MprisListener()..start()),
        ChangeNotifierProvider(create: (context) => AppStateProvider(context.read<OdbService>())),
      ],
      child: Selector<SettingsProvider, ThemeMode>(
        selector: (_, listener) => listener.settings.themeMode,
        builder: (context, status, _) {
          return MaterialApp.router(
            title: 'ODB Dashboard',
            theme: AppTheme.lightTheme,
            darkTheme: AppTheme.darkTheme,
            themeMode: Provider.of<SettingsProvider>(context, listen: true).settings.themeMode,
            routerConfig: AppRouter.router,
            debugShowCheckedModeBanner: false,
          );
        },
      ),
    );
  }
}
