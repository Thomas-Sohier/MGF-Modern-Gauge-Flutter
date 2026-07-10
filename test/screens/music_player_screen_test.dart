import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:modern_gauge_flutter/models/media_info.dart';
import 'package:modern_gauge_flutter/providers/mpris_provider.dart';
import 'package:modern_gauge_flutter/providers/settings_provider.dart';
import 'package:modern_gauge_flutter/services/log_service.dart';
import 'package:modern_gauge_flutter/services/settings_service.dart';
import 'package:modern_gauge_flutter/ui/screens/music_player_screen.dart';
import 'package:modern_gauge_flutter/ui/themes/app_theme.dart';
import 'package:provider/provider.dart';
import 'package:shared_preferences/shared_preferences.dart';

class _FakeMprisListener extends MprisListenerBase {
  @override
  MediaInfo? mediaInfo = MediaInfo(
    title: 'Father Figure',
    artist: 'George Michael',
    duration: const Duration(minutes: 5, seconds: 41),
  );

  @override
  Duration position = const Duration(minutes: 3, seconds: 7);

  @override
  PlaybackStatus playbackStatus = PlaybackStatus.playing;

  @override
  bool get isPlaying => playbackStatus == PlaybackStatus.playing;

  @override
  Future<void> start() async {}
}

void main() {
  setUpAll(() async {
    SharedPreferences.setMockInitialValues({});
    // IO réelle : doit tourner hors de la zone FakeAsync de testWidgets.
    await LogService.initialize();
    await SettingsService().init();
  });

  testWidgets('affiche titre, artiste, position et durée totale', (
    tester,
  ) async {
    await tester.pumpWidget(
      MultiProvider(
        providers: [
          ChangeNotifierProvider<MprisListenerBase>(
            create: (_) => _FakeMprisListener(),
          ),
          ChangeNotifierProvider(create: (_) => SettingsProvider()),
        ],
        child: MaterialApp(
          theme: AppTheme.darkTheme,
          home: const MusicPlayerScreen(),
        ),
      ),
    );
    await tester.pump();

    expect(find.text('Father Figure'), findsOneWidget);
    expect(find.text('George Michael'), findsOneWidget);
    expect(find.text('03:07'), findsOneWidget);
    expect(find.text('05:41'), findsOneWidget);
  });
}
