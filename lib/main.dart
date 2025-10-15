import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/app.dart';
import 'package:modern_gauge_flutter/services/settings_service.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await SettingsService().init();
  runApp(const App());
}
