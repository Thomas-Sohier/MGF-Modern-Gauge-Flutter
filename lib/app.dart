import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/providers/app_state_provider.dart';
import 'package:modern_gauge_flutter/providers/dial_provider.dart';
import 'package:modern_gauge_flutter/providers/settings_provider.dart';
import 'package:modern_gauge_flutter/routes/app_router.dart';
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
        ChangeNotifierProvider(create: (context) => AppStateProvider(context.read<OdbService>())),
        ChangeNotifierProvider(create: (context) => DialProvider(context.read<OdbService>())),
        ChangeNotifierProvider(create: (_) => SettingsProvider()),
        // Ajoute d'autres providers si nécessaire
      ],
      child: Builder(
        builder: (context) {
          return MaterialApp.router(
            title: 'ODB Dashboard',
            theme: AppTheme.lightTheme,
            darkTheme: AppTheme.darkTheme,
            themeMode: ThemeMode.light,
            routerConfig: AppRouter.router,
            debugShowCheckedModeBanner: false,
          );
        },
      ),
    );
  }
}
