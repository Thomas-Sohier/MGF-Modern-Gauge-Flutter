import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:go_router/go_router.dart';
import 'package:mockito/annotations.dart';
import 'package:mockito/mockito.dart';
import 'package:modern_gauge_flutter/app.dart';
import 'package:modern_gauge_flutter/models/settings_data.dart';
import 'package:modern_gauge_flutter/providers/app_state_provider.dart';
import 'package:modern_gauge_flutter/providers/ecu_provider.dart';
import 'package:modern_gauge_flutter/providers/settings_provider.dart';
import 'package:modern_gauge_flutter/services/mpris_listener.dart';

import 'widget_test.mocks.dart';

@GenerateMocks([
  SettingsProvider,
  MprisListener,
  AppStateProvider,
  GoRouter,
  EcuProvider,
])
void main() {
  late MockSettingsProvider mockSettingsProvider;
  late MockMprisListener mockMprisListener;
  late MockAppStateProvider mockAppStateProvider;
  late MockGoRouter mockGoRouter;
  late MockEcuProvider mockEcuProvider;

  setUp(() {
    // Initialisez les mocks avant chaque test
    mockSettingsProvider = MockSettingsProvider();
    mockMprisListener = MockMprisListener();
    mockAppStateProvider = MockAppStateProvider();
    mockGoRouter = MockGoRouter();
    mockEcuProvider = MockEcuProvider();
  });

  testWidgets(
    'App should display with light theme based on mock provider',
    skip: true,
    (WidgetTester tester) async {
      // Arrange
      final mockSettings = SettingsData(themeMode: ThemeMode.light);
      when(mockSettingsProvider.settings).thenReturn(mockSettings);

      // Act
      await tester.pumpWidget(
        App(
          settingsProvider: mockSettingsProvider,
          appStateProvider: mockAppStateProvider,
          mprisListener: mockMprisListener,
          router: mockGoRouter,
          ecuProvider: mockEcuProvider,
        ),
      );

      // Assert
      final materialApp = tester.widget<MaterialApp>(find.byType(MaterialApp));
      expect(materialApp.themeMode, ThemeMode.light);
    },
  );

  testWidgets(
    'App should display with dark theme based on mock provider',
    skip: true,
    (WidgetTester tester) async {
      // Arrange
      final mockSettings = SettingsData(themeMode: ThemeMode.dark);
      when(mockSettingsProvider.settings).thenReturn(mockSettings);

      // Act
      await tester.pumpWidget(
        App(
          settingsProvider: mockSettingsProvider,
          appStateProvider: mockAppStateProvider,
          mprisListener: mockMprisListener,
          router: mockGoRouter,
          ecuProvider: mockEcuProvider,
        ),
      );

      // Assert
      final materialApp = tester.widget<MaterialApp>(find.byType(MaterialApp));
      expect(materialApp.themeMode, ThemeMode.dark);
    },
  );
}
