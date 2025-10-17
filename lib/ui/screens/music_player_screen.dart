import 'dart:io';

import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';
import 'package:marquee/marquee.dart';
import 'package:modern_gauge_flutter/mixins/screen_navigation_mixin.dart';
import 'package:modern_gauge_flutter/providers/mpris_provider.dart';
import 'package:modern_gauge_flutter/routes/navigation_logic.dart';
import 'package:modern_gauge_flutter/routes/route_names.dart';
import 'package:modern_gauge_flutter/services/mpris_listener.dart';
import 'package:modern_gauge_flutter/ui/widgets/music_dial.dart';
import 'package:modern_gauge_flutter/utils/color_util.dart';
import 'package:provider/provider.dart';

// --- STYLES AJUSTÉS POUR LA NOUVELLE INTERFACE ---
const _kInfoTitleTextStyle = TextStyle(color: Colors.black87, fontWeight: FontWeight.bold, fontSize: 18);
const _kInfoTimeTextStyle = TextStyle(
  color: Colors.black,
  fontSize: 18,
  fontWeight: FontWeight.w500,
  fontFamily: 'monospace',
);

String formatDuration(Duration d) {
  if (d.inSeconds <= 0) return '00:00';
  String twoDigits(int n) => n.toString().padLeft(2, '0');
  if (d.inHours >= 1) {
    final minutes = twoDigits(d.inMinutes.remainder(60));
    return "${d.inHours}:$minutes";
  } else {
    final minutes = twoDigits(d.inMinutes.remainder(60));
    final seconds = twoDigits(d.inSeconds.remainder(60));
    return "$minutes:$seconds";
  }
}

// --- CLASSE PRINCIPALE DE L'ÉCRAN ---
class MusicPlayerScreen extends StatefulWidget {
  const MusicPlayerScreen({super.key});
  @override
  State<MusicPlayerScreen> createState() => _MusicPlayerScreenState();
}

class _MusicPlayerScreenState extends State<MusicPlayerScreen> with ScreenNavigationMixin<MusicPlayerScreen> {
  @override
  void nextScreen() {
    const currentRoute = RouteNames.dashboardRoute + RouteNames.musicRoute;
    context.go(getNextRoute(currentRoute));
  }

  @override
  void previousScreen() {
    const currentRoute = RouteNames.dashboardRoute + RouteNames.musicRoute;
    context.go(getPreviousRoute(currentRoute));
  }

  @override
  Widget build(BuildContext context) {
    return buildNavigableScreen(
      child: Selector<MprisListenerBase, PlaybackStatus>(
        selector: (_, listener) => listener.playbackStatus,
        builder: (context, status, _) {
          if (status == PlaybackStatus.stopped) {
            return _NoMusicPlayerUI();
          }
          return _MusicPlayerUI();
        },
      ),
    );
  }
}

class _NoMusicPlayerUI extends StatelessWidget {
  const _NoMusicPlayerUI();

  @override
  Widget build(BuildContext context) {
    return Center(
      child: DecoratedBox(
        decoration: BoxDecoration(
          shape: BoxShape.circle,
          gradient: LinearGradient(
            colors: [Colors.deepPurple.shade700, Colors.teal.shade400],
            begin: Alignment.topLeft,
            end: Alignment.bottomRight,
          ),
          boxShadow: [
            BoxShadow(color: Colors.black.withAlpha(90), spreadRadius: 2, blurRadius: 10, offset: const Offset(0, 5)),
          ],
        ),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          crossAxisAlignment: CrossAxisAlignment.center,
          children: [
            const Icon(Icons.headset_off_outlined, color: Colors.white, size: 100),
            const SizedBox(height: 30),
            const Text(
              "Aucun lecteur de musique actif détecté...",
              textAlign: TextAlign.center,
              style: TextStyle(color: Colors.white70, fontSize: 18, fontWeight: FontWeight.w500),
            ),
          ],
        ),
      ),
    );
  }
}

// --- STRUCTURE DE L'INTERFACE ---

class _MusicPlayerUI extends StatelessWidget {
  const _MusicPlayerUI();

  @override
  Widget build(BuildContext context) {
    return Stack(alignment: Alignment.center, children: const [_AlbumArt(), _ProgressDial(), _InfoPanel()]);
  }
}

class _InfoPanel extends StatelessWidget {
  const _InfoPanel();

  @override
  Widget build(BuildContext context) {
    return Positioned(
      bottom: 0,
      height: 80,
      width: 300,
      child: Container(
        color: Colors.white,
        child: Stack(
          alignment: Alignment.center,
          children: [
            Positioned(
              top: 5,
              child: ConstrainedBox(
                constraints: const BoxConstraints(maxHeight: 25, maxWidth: 200),
                child: _TitleText(),
              ),
            ),
            Positioned(
              top: 25,
              child: ConstrainedBox(
                constraints: const BoxConstraints(maxWidth: 160),
                child: Row(
                  mainAxisSize: MainAxisSize.max,
                  spacing: 6,
                  crossAxisAlignment: CrossAxisAlignment.center,
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: const [_CurrentPositionText(), _PlaybackStatusIndicator(), _TotalDurationText()],
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}

// --- WIDGETS ATOMIQUES ---
class _TitleText extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    return Selector<MprisListenerBase, String>(
      selector: (_, listener) => listener.mediaInfo?.title ?? 'Titre inconnu',
      builder: (_, title, __) => Marquee(
        text: title,
        blankSpace: 60,
        style: _kInfoTitleTextStyle,
        crossAxisAlignment: CrossAxisAlignment.center,
        pauseAfterRound: Duration(seconds: 2),
        showFadingOnlyWhenScrolling: true,
        fadingEdgeStartFraction: 0.12,
        fadingEdgeEndFraction: 0.12,
      ),
    );
  }
}

class _CurrentPositionText extends StatelessWidget {
  const _CurrentPositionText();

  @override
  Widget build(BuildContext context) {
    return Selector<MprisListenerBase, Duration>(
      selector: (_, listener) => listener.position,
      builder: (_, position, __) => Text(formatDuration(position), style: _kInfoTimeTextStyle),
    );
  }
}

class _PlaybackStatusIndicator extends StatelessWidget {
  const _PlaybackStatusIndicator();

  @override
  Widget build(BuildContext context) {
    return Selector<MprisListenerBase, PlaybackStatus>(
      selector: (_, listener) => listener.playbackStatus,
      builder: (_, status, __) {
        final iconData = status == PlaybackStatus.playing ? Icons.pause_rounded : Icons.play_arrow_rounded;
        return Icon(iconData, color: Colors.black54, size: 30);
      },
    );
  }
}

class _TotalDurationText extends StatelessWidget {
  const _TotalDurationText();

  @override
  Widget build(BuildContext context) {
    return Selector<MprisListenerBase, Duration>(
      selector: (_, listener) => listener.mediaInfo?.duration ?? Duration.zero,
      builder: (_, totalDuration, __) => Text(formatDuration(totalDuration), style: _kInfoTimeTextStyle),
    );
  }
}

// --- EXTRACTION DE COULEUR AVEC CACHE ---
class _ColorExtractorService {
  static final _instance = _ColorExtractorService._();
  factory _ColorExtractorService() => _instance;
  _ColorExtractorService._();

  final Map<String, ColorScheme> _colorCache = {};

  Future<ColorScheme> extractDominantColor(String? artUrl) async {
    if (artUrl == null || artUrl.isEmpty) {
      return ColorScheme.fromSeed(seedColor: Colors.blue);
    }

    if (_colorCache.containsKey(artUrl)) {
      return _colorCache[artUrl]!;
    }

    try {
      final imageProvider = FileImage(File(artUrl));
      final palette = await ColorUtil().getColorsFromImage(imageProvider);

      final colorScheme = ColorScheme.fromSeed(seedColor: palette[0]);
      _colorCache[artUrl] = colorScheme;
      return colorScheme;
    } catch (e) {
      return ColorScheme.fromSeed(seedColor: Colors.blue);
    }
  }

  void clearCache() {
    _colorCache.clear();
  }
}

class _AlbumArt extends StatelessWidget {
  const _AlbumArt();

  @override
  Widget build(BuildContext context) {
    return Selector<MprisListenerBase, String?>(
      selector: (_, listener) => listener.mediaInfo?.artUrl,
      builder: (_, artUrl, __) {
        const icon = Icon(Icons.music_note, size: 150, color: Colors.grey);
        return Container(
          decoration: const BoxDecoration(shape: BoxShape.circle, color: Color(0xFFE0E5E8)),
          child: ClipRRect(
            borderRadius: const BorderRadius.all(Radius.circular(1000)),
            child: artUrl != null
                ? Image.file(
                    File(artUrl),
                    fit: BoxFit.cover,
                    width: double.infinity,
                    height: double.infinity,
                    gaplessPlayback: true,
                    filterQuality: FilterQuality.high,
                    errorBuilder: (_, __, ___) => icon,
                  )
                : icon,
          ),
        );
      },
    );
  }
}

// --- CADRAN AVEC COULEUR DYNAMIQUE ---
class _ProgressDial extends StatelessWidget {
  const _ProgressDial();

  @override
  Widget build(BuildContext context) {
    return Selector<MprisListenerBase, (Duration, Duration, String?)>(
      selector: (_, listener) =>
          (listener.position, listener.mediaInfo?.duration ?? Duration.zero, listener.mediaInfo?.artUrl),
      builder: (_, data, __) {
        final (position, totalDuration, artUrl) = data;
        final progress = (totalDuration.inMilliseconds > 0)
            ? position.inMilliseconds / totalDuration.inMilliseconds
            : 0.0;

        return _DynamicColorDial(
          position: position,
          totalDuration: totalDuration,
          progress: progress.clamp(0.0, 1.0),
          artUrl: artUrl,
        );
      },
    );
  }
}

class _DynamicColorDial extends StatefulWidget {
  final Duration position;
  final Duration totalDuration;
  final double progress;
  final String? artUrl;

  const _DynamicColorDial({
    required this.position,
    required this.totalDuration,
    required this.progress,
    required this.artUrl,
  });

  @override
  State<_DynamicColorDial> createState() => _DynamicColorDialState();
}

class _DynamicColorDialState extends State<_DynamicColorDial> {
  Color _foregroundColor = Colors.blue;
  Color _containerColor = Colors.white;

  @override
  void initState() {
    super.initState();
    _extractColor();
  }

  @override
  void didUpdateWidget(_DynamicColorDial oldWidget) {
    super.didUpdateWidget(oldWidget);
    // Si l'artUrl change, extraire la nouvelle couleur
    if (oldWidget.artUrl != widget.artUrl) {
      _extractColor();
    }
  }

  Future<ColorScheme> _extractColor() async {
    final colorScheme = await _ColorExtractorService().extractDominantColor(widget.artUrl);
    if (mounted) {
      setState(() {
        _foregroundColor = colorScheme.primary;
        _containerColor = colorScheme.primaryContainer;
      });
    }
    return colorScheme;
  }

  @override
  Widget build(BuildContext context) {
    return CustomPaint(
      size: Size.infinite,
      painter: MusicDial(
        progress: widget.progress,
        foregroundColor: _foregroundColor,
        backgroundColor: _containerColor.withAlpha(160),
        sweepFactor: 0.67,
        strokeWidth: 14,
      ),
    );
  }
}
