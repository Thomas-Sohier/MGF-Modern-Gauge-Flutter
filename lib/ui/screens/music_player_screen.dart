import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';
import 'package:modern_gauge_flutter/mixins/screen_navigation_mixin.dart';
import 'package:modern_gauge_flutter/routes/navigation_logic.dart';
import 'package:modern_gauge_flutter/routes/route_names.dart';
import 'package:modern_gauge_flutter/ui/widgets/music_dial.dart';

class MusicPlayerScreen extends StatefulWidget {
  const MusicPlayerScreen({super.key});

  @override
  State<MusicPlayerScreen> createState() => _MusicPlayerScreenState();
}

class _MusicPlayerScreenState extends State<MusicPlayerScreen> with ScreenNavigationMixin<MusicPlayerScreen> {
  final double _progress = 30.0 / 176.0;
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
    final double diameter = MediaQuery.of(context).size.width * 0.9;

    return buildNavigableScreen(
      child: SizedBox(
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
                    borderRadius: BorderRadius.all(Radius.circular(1000)),
                    child: Image.asset('assets/images/album_art.jpg'),
                  ),
                  Positioned(
                    top: diameter * 0.12,
                    child: const Text(
                      'K A N Y E   W E S T',
                      style: TextStyle(
                        color: Color(0xFF5A5A5A),
                        fontWeight: FontWeight.w600,
                        fontSize: 14,
                        letterSpacing: 3,
                      ),
                    ),
                  ),

                  // Nom de l'album
                  Positioned(
                    bottom: diameter * 0.12,
                    child: const Text(
                      '808s & HEARTBREAK',
                      style: TextStyle(
                        color: Color(0xFF5A5A5A),
                        fontWeight: FontWeight.w600,
                        fontSize: 14,
                        letterSpacing: 1.5,
                      ),
                    ),
                  ),

                  // Temps actuel
                  Positioned(
                    left: diameter * 0.12,
                    bottom: diameter * 0.22,
                    child: const Text(
                      '0:30',
                      style: TextStyle(color: Colors.black, fontSize: 28, fontWeight: FontWeight.w500),
                    ),
                  ),

                  // Durée totale
                  Positioned(
                    right: diameter * 0.12,
                    bottom: diameter * 0.22,
                    child: const Text(
                      '2:56',
                      style: TextStyle(color: Colors.black, fontSize: 28, fontWeight: FontWeight.w500),
                    ),
                  ),
                ],
              ),
            ),
            CustomPaint(
              size: Size.infinite,
              painter: MusicDial(
                progress: 0.8,
                foregroundColor: Colors.deepPurple,
                backgroundColor: Colors.grey,
                sweepFactor: 0.7,
              ),
            ),
          ],
        ),
      ),
    );
  }
}
