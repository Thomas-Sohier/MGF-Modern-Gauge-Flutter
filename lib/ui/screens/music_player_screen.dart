import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';
import 'package:modern_gauge_flutter/mixins/screen_navigation_mixin.dart';
import 'package:modern_gauge_flutter/providers/mpris_provider.dart';
import 'package:modern_gauge_flutter/routes/navigation_logic.dart';
import 'package:modern_gauge_flutter/routes/route_names.dart';
import 'package:modern_gauge_flutter/ui/widgets/music_dial.dart';
import 'package:provider/provider.dart';

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

  String _formatDuration(Duration d) {
    if (d == Duration.zero) return '0:00';
    return d.toString().split('.').first.substring(2);
  }

  @override
  Widget build(BuildContext context) {
    final double diameter = MediaQuery.of(context).size.width * 0.9;
    final listener = context.watch<MprisListenerBase>();
    return buildNavigableScreen(
      child: AnimatedBuilder(
        animation: listener,
        builder: (context, child) {
          final info = listener.mediaInfo;
          final position = listener.position;
          final totalDuration = info?.duration ?? Duration.zero;

          final progress = (totalDuration.inMilliseconds > 0)
              ? position.inMilliseconds / totalDuration.inMilliseconds
              : 0.0;

          // 5. Si aucune information n'est disponible, on affiche un message
          if (info == null || info.title == null) {
            return const Center(
              child: Text("Aucun lecteur de musique actif détecté...", style: TextStyle(color: Colors.white)),
            );
          }

          return SizedBox(
            width: diameter,
            height: diameter,
            child: Stack(
              alignment: Alignment.center,
              children: [
                Container(
                  decoration: const BoxDecoration(shape: BoxShape.circle, color: Color(0xFFE0E5E8)),
                  child: Stack(
                    alignment: Alignment.center,
                    children: [
                      ClipRRect(
                        borderRadius: const BorderRadius.all(Radius.circular(1000)),
                        child: info.artUrl != null
                            ? Image.network(
                                info.artUrl!,
                                fit: BoxFit.cover,
                                width: double.infinity,
                                height: double.infinity,
                                errorBuilder: (context, error, stackTrace) =>
                                    const Icon(Icons.music_note, size: 150, color: Colors.grey),
                              )
                            : const Icon(Icons.music_note, size: 150, color: Colors.grey),
                      ),
                      Positioned(
                        top: diameter * 0.12,
                        child: Text(
                          info.artist?.toUpperCase() ?? 'ARTISTE INCONNU',
                          style: const TextStyle(
                            color: Color(0xFF5A5A5A),
                            fontWeight: FontWeight.w600,
                            fontSize: 14,
                            letterSpacing: 3,
                          ),
                        ),
                      ),
                      Positioned(
                        bottom: diameter * 0.12,
                        child: Text(
                          info.album?.toUpperCase() ?? 'ALBUM INCONNU',
                          style: const TextStyle(
                            color: Color(0xFF5A5A5A),
                            fontWeight: FontWeight.w600,
                            fontSize: 14,
                            letterSpacing: 1.5,
                          ),
                        ),
                      ),
                      Positioned(
                        left: diameter * 0.12,
                        bottom: diameter * 0.22,
                        child: Text(
                          _formatDuration(position),
                          style: const TextStyle(color: Colors.black, fontSize: 28, fontWeight: FontWeight.w500),
                        ),
                      ),
                      Positioned(
                        right: diameter * 0.12,
                        bottom: diameter * 0.22,
                        child: Text(
                          _formatDuration(totalDuration),
                          style: const TextStyle(color: Colors.black, fontSize: 28, fontWeight: FontWeight.w500),
                        ),
                      ),
                    ],
                  ),
                ),
                CustomPaint(
                  size: Size.infinite,
                  painter: MusicDial(
                    progress: progress,
                    foregroundColor: Colors.deepPurple,
                    backgroundColor: Colors.grey,
                    sweepFactor: 0.7,
                  ),
                ),
              ],
            ),
          );
        },
      ),
    );
  }
}
