import 'dart:async';
import 'dart:io';
import 'dart:math';
import 'dart:ui';

import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';
import 'package:modern_gauge_flutter/mixins/screen_navigation_mixin.dart';
import 'package:modern_gauge_flutter/providers/mpris_provider.dart';
import 'package:modern_gauge_flutter/routes/navigation_logic.dart';
import 'package:modern_gauge_flutter/routes/route_names.dart';
import 'package:modern_gauge_flutter/models/media_info.dart';
import 'package:modern_gauge_flutter/ui/themes/music_player_theme.dart';
import 'package:modern_gauge_flutter/ui/widgets/music_dial.dart';
import 'package:provider/provider.dart';

// ── Dimensions du design ─────────────────────────────────────────────────────
// La maquette est dessinée pour un écran rond de 480 px ; chaque valeur
// ci-dessous est exprimée dans ce repère puis multipliée par `scale`
// (= côté réel / 480) pour rester proportionnelle sur toute résolution.
/// Diamètre de référence de la maquette.
const _kDesignSize = 480.0;

/// Marge entre le bord de l'écran et l'anneau de progression.
const _kRingPadding = 6.0;

/// Épaisseur du trait de l'anneau de progression.
const _kRingStroke = 12.0;

/// Marge horizontale du bloc central (évite que le texte touche l'anneau).
const _kContentHPadding = 60.0;

/// Diamètre de la pochette ronde centrale.
const _kArtSize = 190.0;

/// Espacements verticaux : pochette→titre, titre→artiste, artiste→ligne temps.
const _kArtTitleGap = 4.0;
const _kTitleArtistGap = 0;
const _kArtistTimeGap = 20.0;

/// Ligne du bas : largeur réservée aux temps (stabilise le centrage quand le
/// texte passe de 0:59 à 1:00), écart temps↔bouton, diamètre du bouton et
/// taille de son icône.
const _kTimeTextWidth = 70.0;
const _kTimeButtonGap = 12.0;
const _kPlayButtonSize = 80.0;
const _kPlayIconSize = 28.0;

/// Police embarquée dans les assets : la police système par défaut n'existe
/// pas sur l'image du Pi (pas de fontconfig), le texte ne s'y rendrait pas.
const _kFontFamily = 'JetBrainsMono';

/// Tailles de police : titre, artiste, temps.
const _kTitleFontSize = 30.0;
const _kArtistFontSize = 22.0;
const _kTimeFontSize = 20.0;

/// Interlignes, et hauteurs réservées aux textes : le titre occupe toujours
/// la place de 2 lignes et l'artiste d'1 ligne, pour que la mise en page ne
/// saute pas quand le morceau change.
const _kTitleLineHeight = 1.2;
const _kArtistLineHeight = 1.35;
const _kTitleBoxHeight = _kTitleFontSize * _kTitleLineHeight * 2;
const _kArtistBoxHeight = _kArtistFontSize * _kArtistLineHeight;

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

class _MusicPlayerScreenState extends State<MusicPlayerScreen>
    with ScreenNavigationMixin<MusicPlayerScreen> {
  @override
  void nextScreen() {
    const currentRoute = RouteNames.musicFull;
    context.go(getNextRoute(currentRoute, enabledScreens));
  }

  @override
  void previousScreen() {
    const currentRoute = RouteNames.musicFull;
    context.go(getPreviousRoute(currentRoute, enabledScreens));
  }

  @override
  Widget build(BuildContext context) {
    return buildNavigableScreen(
      child: Selector<MprisListenerBase, PlaybackStatus>(
        selector: (_, listener) => listener.playbackStatus,
        builder: (context, status, _) => _PlayerOrEmpty(status: status),
      ),
    );
  }
}

/// L'agent envoie brièvement `stopped` entre deux morceaux : on ne bascule
/// sur l'écran « aucun lecteur » que si l'état persiste, pour éviter le
/// clignotement au changement de musique. Le retour à la lecture est immédiat.
class _PlayerOrEmpty extends StatefulWidget {
  final PlaybackStatus status;
  const _PlayerOrEmpty({required this.status});

  @override
  State<_PlayerOrEmpty> createState() => _PlayerOrEmptyState();
}

class _PlayerOrEmptyState extends State<_PlayerOrEmpty> {
  static const _stoppedDebounce = Duration(milliseconds: 1500);

  Timer? _timer;
  late bool _showEmpty = widget.status == PlaybackStatus.stopped;

  @override
  void didUpdateWidget(_PlayerOrEmpty oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (widget.status == oldWidget.status) return;
    _timer?.cancel();
    if (widget.status == PlaybackStatus.stopped) {
      _timer = Timer(_stoppedDebounce, () {
        if (mounted) setState(() => _showEmpty = true);
      });
    } else if (_showEmpty) {
      setState(() => _showEmpty = false);
    }
  }

  @override
  void dispose() {
    _timer?.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) =>
      _showEmpty ? const _NoMusicPlayerUI() : const _MusicPlayerUI();
}

class _NoMusicPlayerUI extends StatelessWidget {
  const _NoMusicPlayerUI();

  @override
  Widget build(BuildContext context) {
    return const Column(
      mainAxisAlignment: MainAxisAlignment.center,
      crossAxisAlignment: CrossAxisAlignment.center,
      children: [
        Icon(Icons.headset_off_outlined, size: 100),
        SizedBox(height: 30),
        Text(
          "Aucun lecteur de musique actif détecté...",
          textAlign: TextAlign.center,
        ),
      ],
    );
  }
}

// --- STRUCTURE DE L'INTERFACE ---

/// Met le lecteur à l'échelle (cf. constantes `_kDesignSize`…) dans un carré
/// centré : sur une zone non carrée (fenêtre desktop), un ClipOval direct
/// découperait une ellipse et l'anneau — dont le rayon suit la largeur —
/// déborderait de l'écran.
class _MusicPlayerUI extends StatelessWidget {
  const _MusicPlayerUI();

  @override
  Widget build(BuildContext context) {
    final colors = Theme.of(context).extension<MusicPlayerTheme>()!;
    return LayoutBuilder(
      builder: (context, constraints) {
        final side = constraints.biggest.shortestSide;
        final scale = side / _kDesignSize;
        return Center(
          child: SizedBox.square(
            dimension: side,
            child: ClipOval(
              child: ColoredBox(
                color: colors.surface,
                child: Stack(
                  alignment: Alignment.center,
                  children: [
                    const _BlurredArtBackground(),
                    Padding(
                      padding: EdgeInsets.all(_kRingPadding * scale),
                      child: _ProgressRing(scale: scale),
                    ),
                    _CenterContent(scale: scale),
                  ],
                ),
              ),
            ),
          ),
        );
      },
    );
  }
}

class _CenterContent extends StatelessWidget {
  final double scale;
  const _CenterContent({required this.scale});

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: EdgeInsets.symmetric(horizontal: _kContentHPadding * scale),
      child: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          _AlbumArt(size: _kArtSize * scale),
          SizedBox(height: _kArtTitleGap * scale),
          SizedBox(
            height: _kTitleBoxHeight * scale,
            child: Center(child: _TitleText(scale: scale)),
          ),
          SizedBox(height: _kTitleArtistGap * scale),
          SizedBox(
            height: _kArtistBoxHeight * scale,
            child: Center(child: _ArtistText(scale: scale)),
          ),
          SizedBox(height: _kArtistTimeGap * scale),
          Row(
            mainAxisSize: MainAxisSize.min,
            children: [
              SizedBox(
                width: _kTimeTextWidth * scale,
                child: _CurrentPositionText(scale: scale),
              ),
              SizedBox(width: _kTimeButtonGap * scale),
              _PlaybackStatusIndicator(scale: scale),
              SizedBox(width: _kTimeButtonGap * scale),
              SizedBox(
                width: _kTimeTextWidth * scale,
                child: _TotalDurationText(scale: scale),
              ),
            ],
          ),
        ],
      ),
    );
  }
}

// --- WIDGETS ATOMIQUES ---
class _TitleText extends StatelessWidget {
  final double scale;
  const _TitleText({required this.scale});

  @override
  Widget build(BuildContext context) {
    final colors = Theme.of(context).extension<MusicPlayerTheme>()!;
    return Selector<MprisListenerBase, String>(
      selector: (_, listener) => listener.mediaInfo?.title ?? 'Titre inconnu',
      builder: (_, title, __) => Text(
        title,
        textAlign: TextAlign.center,
        maxLines: 2,
        overflow: TextOverflow.ellipsis,
        style: TextStyle(
          fontFamily: _kFontFamily,
          color: colors.title,
          fontWeight: FontWeight.bold,
          fontSize: _kTitleFontSize * scale,
          height: _kTitleLineHeight,
        ),
      ),
    );
  }
}

class _ArtistText extends StatelessWidget {
  final double scale;
  const _ArtistText({required this.scale});

  @override
  Widget build(BuildContext context) {
    final colors = Theme.of(context).extension<MusicPlayerTheme>()!;
    return Selector<MprisListenerBase, String>(
      selector: (_, listener) => listener.mediaInfo?.artist ?? '',
      builder: (_, artist, __) => Text(
        artist,
        textAlign: TextAlign.center,
        maxLines: 1,
        overflow: TextOverflow.ellipsis,
        style: TextStyle(
          fontFamily: _kFontFamily,
          color: colors.subtle,
          fontSize: _kArtistFontSize * scale,
          height: _kArtistLineHeight,
        ),
      ),
    );
  }
}

class _CurrentPositionText extends StatelessWidget {
  final double scale;
  const _CurrentPositionText({required this.scale});

  @override
  Widget build(BuildContext context) {
    final colors = Theme.of(context).extension<MusicPlayerTheme>()!;
    return Selector<MprisListenerBase, Duration>(
      selector: (_, listener) => listener.position,
      builder: (_, position, __) => Text(
        formatDuration(position),
        textAlign: TextAlign.right,
        style: TextStyle(
          fontFamily: _kFontFamily,
          color: colors.subtle,
          fontSize: _kTimeFontSize * scale,
          fontWeight: FontWeight.w500,
        ),
      ),
    );
  }
}

/// Bouton central du design : simple indicateur (MPRIS est en écoute seule).
class _PlaybackStatusIndicator extends StatelessWidget {
  final double scale;
  const _PlaybackStatusIndicator({required this.scale});

  @override
  Widget build(BuildContext context) {
    final colors = Theme.of(context).extension<MusicPlayerTheme>()!;
    return Selector<MprisListenerBase, PlaybackStatus>(
      selector: (_, listener) => listener.playbackStatus,
      builder: (_, status, __) {
        final iconData = status == PlaybackStatus.playing
            ? Icons.pause_rounded
            : Icons.play_arrow_rounded;
        return Container(
          width: _kPlayButtonSize * scale,
          height: _kPlayButtonSize * scale,
          decoration: BoxDecoration(
            shape: BoxShape.circle,
            color: colors.primary,
          ),
          child: Icon(
            iconData,
            color: colors.surface,
            size: _kPlayIconSize * scale,
          ),
        );
      },
    );
  }
}

class _TotalDurationText extends StatelessWidget {
  final double scale;
  const _TotalDurationText({required this.scale});

  @override
  Widget build(BuildContext context) {
    final colors = Theme.of(context).extension<MusicPlayerTheme>()!;
    return Selector<MprisListenerBase, Duration>(
      selector: (_, listener) => listener.mediaInfo?.duration ?? Duration.zero,
      builder: (_, totalDuration, __) => Text(
        formatDuration(totalDuration),
        textAlign: TextAlign.left,
        style: TextStyle(
          fontFamily: _kFontFamily,
          color: colors.subtle,
          fontSize: _kTimeFontSize * scale,
          fontWeight: FontWeight.w500,
        ),
      ),
    );
  }
}

Widget _artImage(String artUrl, {required Widget fallback}) {
  final isNetwork =
      artUrl.startsWith('http://') || artUrl.startsWith('https://');
  return isNetwork
      ? Image.network(
          artUrl,
          fit: BoxFit.cover,
          width: double.infinity,
          height: double.infinity,
          filterQuality: FilterQuality.medium,
          gaplessPlayback: true,
          errorBuilder: (_, __, ___) => fallback,
        )
      : Image.file(
          File(artUrl),
          fit: BoxFit.cover,
          width: double.infinity,
          height: double.infinity,
          gaplessPlayback: true,
          filterQuality: FilterQuality.medium,
          errorBuilder: (_, __, ___) => fallback,
        );
}

/// Pochette floutée et assombrie en fond plein écran.
class _BlurredArtBackground extends StatelessWidget {
  const _BlurredArtBackground();

  @override
  Widget build(BuildContext context) {
    final colors = Theme.of(context).extension<MusicPlayerTheme>()!;
    return Selector<MprisListenerBase, String?>(
      selector: (_, listener) => listener.mediaInfo?.artUrl,
      builder: (_, artUrl, __) {
        if (artUrl == null) return const SizedBox.shrink();
        return RepaintBoundary(
          child: ImageFiltered(
            imageFilter: ImageFilter.blur(sigmaX: 24, sigmaY: 24),
            child: Transform.scale(
              scale: 1.25,
              child: ColorFiltered(
                colorFilter: ColorFilter.mode(
                  colors.surface.withValues(alpha: 0.4),
                  BlendMode.srcOver,
                ),
                child: _artImage(artUrl, fallback: const SizedBox.shrink()),
              ),
            ),
          ),
        );
      },
    );
  }
}

class _AlbumArt extends StatelessWidget {
  final double size;
  const _AlbumArt({required this.size});

  @override
  Widget build(BuildContext context) {
    final colors = Theme.of(context).extension<MusicPlayerTheme>()!;
    return Selector<MprisListenerBase, String?>(
      selector: (_, listener) => listener.mediaInfo?.artUrl,
      builder: (_, artUrl, __) {
        final fallback = Icon(
          Icons.music_note,
          size: size * 0.6,
          color: colors.subtle,
        );
        return Container(
          width: size,
          height: size,
          clipBehavior: Clip.antiAlias,
          decoration: BoxDecoration(
            shape: BoxShape.circle,
            color: colors.artFallback,
          ),
          child: artUrl == null
              ? fallback
              : _artImage(artUrl, fallback: fallback),
        );
      },
    );
  }
}

// --- ANNEAU DE PROGRESSION PÉRIPHÉRIQUE ---
class _ProgressRing extends StatelessWidget {
  final double scale;
  const _ProgressRing({required this.scale});

  @override
  Widget build(BuildContext context) {
    final colors = Theme.of(context).extension<MusicPlayerTheme>()!;
    return Selector<MprisListenerBase, (Duration, Duration)>(
      selector: (_, listener) =>
          (listener.position, listener.mediaInfo?.duration ?? Duration.zero),
      builder: (_, data, __) {
        final (position, totalDuration) = data;
        final progress = (totalDuration.inMilliseconds > 0)
            ? position.inMilliseconds / totalDuration.inMilliseconds
            : 0.0;

        // MusicDial démarre en bas ; le design démarre en haut (12 h).
        return Transform.rotate(
          angle: pi,
          child: MusicDial(
            progress: progress.clamp(0.0, 1.0),
            foregroundColor: colors.primary,
            backgroundColor: colors.ringTrack,
            sweepFactor: 1.0,
            strokeWidth: _kRingStroke * scale,
          ),
        );
      },
    );
  }
}
