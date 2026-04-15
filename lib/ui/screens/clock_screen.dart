import 'dart:async';
import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';
import 'package:modern_gauge_flutter/mixins/screen_navigation_mixin.dart';
import 'package:modern_gauge_flutter/routes/navigation_logic.dart';
import 'package:modern_gauge_flutter/routes/route_names.dart';
import 'package:modern_gauge_flutter/ui/widgets/analog_clock.dart';

class ClockScreen extends StatefulWidget {
  const ClockScreen({super.key});

  @override
  State<ClockScreen> createState() => _ClockScreenState();
}

class _ClockScreenState extends State<ClockScreen>
    with ScreenNavigationMixin<ClockScreen> {
  @override
  void nextScreen() {
    const currentRoute = RouteNames.dashboardRoute + RouteNames.timeRoute;
    context.go(getNextRoute(currentRoute, enabledScreens));
  }

  @override
  void previousScreen() {
    const currentRoute = RouteNames.dashboardRoute + RouteNames.timeRoute;
    context.go(getPreviousRoute(currentRoute, enabledScreens));
  }

  late DateTime _currentTime;
  Timer? _timer;

  @override
  void initState() {
    super.initState();
    _currentTime = _nowTruncatedToMinute();
    _scheduleNextMinuteTick();
  }

  @override
  void dispose() {
    _timer?.cancel();
    super.dispose();
  }

  static DateTime _nowTruncatedToMinute() {
    final now = DateTime.now();
    return DateTime(now.year, now.month, now.day, now.hour, now.minute);
  }

  /// Schedules the first tick to fire exactly at the next minute boundary,
  /// then switches to a periodic 1-minute timer.
  void _scheduleNextMinuteTick() {
    final now = DateTime.now();
    final msUntilNextMinute =
        (60 - now.second) * 1000 - now.millisecond;

    _timer = Timer(Duration(milliseconds: msUntilNextMinute), () {
      _tick();
      _timer = Timer.periodic(const Duration(minutes: 1), (_) => _tick());
    });
  }

  void _tick() {
    if (!mounted) return;
    setState(() => _currentTime = _nowTruncatedToMinute());
  }

  @override
  Widget build(BuildContext context) {
    return buildNavigableScreen(child: AnalogClock(dateTime: _currentTime));
  }
}
