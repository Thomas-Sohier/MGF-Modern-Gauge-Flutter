import 'dart:async';

import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';
import 'package:modern_gauge_flutter/models/alert_info.dart';
import 'package:modern_gauge_flutter/providers/settings_provider.dart';
import 'package:modern_gauge_flutter/routes/route_names.dart';
import 'package:modern_gauge_flutter/services/notification_server_listener.dart';
import 'package:modern_gauge_flutter/ui/widgets/circular_content.dart';
import 'package:modern_gauge_flutter/ui/themes/app_text_styles.dart';
import 'package:provider/provider.dart';

/// Wraps the routed [child] and shows an incoming-notification screen in front
/// of it whenever a one-shot alert arrives, except while the settings screen
/// (or one of its subscreens) is showing.
///
/// The overlay auto-dismisses after the user-configured duration
/// (`SettingsData.notificationDurationSeconds`), can be tapped to dismiss early,
/// and is dismissed if the user navigates into settings while it is visible.
class NotificationOverlayHost extends StatefulWidget {
  final Widget child;
  final GoRouter router;

  const NotificationOverlayHost({
    super.key,
    required this.child,
    required this.router,
  });

  @override
  State<NotificationOverlayHost> createState() =>
      _NotificationOverlayHostState();
}

class _NotificationOverlayHostState extends State<NotificationOverlayHost> {
  StreamSubscription<AlertInfo>? _subscription;
  Timer? _dismissTimer;
  AlertInfo? _current;
  bool _subscribed = false;

  @override
  void didChangeDependencies() {
    super.didChangeDependencies();
    if (_subscribed) return;
    _subscribed = true;
    _subscription = context.read<NotificationServerListener>().alerts.listen(
      _onAlert,
    );
    widget.router.routerDelegate.addListener(_onRouteChanged);
  }

  bool get _onSettings =>
      widget.router.state.matchedLocation.startsWith(RouteNames.settingsRoute);

  void _onAlert(AlertInfo alert) {
    // Suppressed on the settings screen and its subscreens.
    if (_onSettings) return;
    final seconds = context
        .read<SettingsProvider>()
        .settings
        .notificationDurationSeconds;
    setState(() => _current = alert);
    _dismissTimer?.cancel();
    _dismissTimer = Timer(Duration(seconds: seconds), _dismiss);
  }

  void _onRouteChanged() {
    // Drop the overlay if the user navigates into settings while it is showing.
    if (_current != null && _onSettings) _dismiss();
  }

  void _dismiss() {
    _dismissTimer?.cancel();
    _dismissTimer = null;
    if (mounted) setState(() => _current = null);
  }

  @override
  void dispose() {
    _subscription?.cancel();
    _dismissTimer?.cancel();
    widget.router.routerDelegate.removeListener(_onRouteChanged);
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final current = _current;
    return Stack(
      children: [
        widget.child,
        AnimatedSwitcher(
          duration: const Duration(milliseconds: 200),
          transitionBuilder: (child, animation) => FadeTransition(
            opacity: animation,
            child: ScaleTransition(
              scale: Tween<double>(begin: 0.96, end: 1).animate(animation),
              child: child,
            ),
          ),
          child: current == null
              ? const SizedBox.shrink()
              : _NotificationScreen(
                  key: ValueKey(current),
                  alert: current,
                  onDismiss: _dismiss,
                ),
        ),
      ],
    );
  }
}

/// Full-screen notification shown in front of the current screen.
class _NotificationScreen extends StatelessWidget {
  final AlertInfo alert;
  final VoidCallback onDismiss;

  const _NotificationScreen({
    super.key,
    required this.alert,
    required this.onDismiss,
  });

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final primary = theme.primaryColor;
    return Material(
      type: MaterialType.transparency,
      child: GestureDetector(
        onTap: onDismiss,
        behavior: HitTestBehavior.opaque,
        child: CircularContent(
          child: ColoredBox(
            color: theme.scaffoldBackgroundColor,
            child: Center(
              child: Padding(
                padding: const EdgeInsets.symmetric(horizontal: 40),
                child: Column(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    Icon(Icons.notifications_active, size: 44, color: primary),
                    const SizedBox(height: 16),
                    if (alert.app.isNotEmpty)
                      Text(
                        alert.app.toUpperCase(),
                        textAlign: TextAlign.center,
                        style: AppTextStyles.small.copyWith(color: primary),
                      ),
                    if (alert.title.isNotEmpty) ...[
                      const SizedBox(height: 8),
                      Text(
                        alert.title,
                        textAlign: TextAlign.center,
                        maxLines: 2,
                        overflow: TextOverflow.ellipsis,
                        style: AppTextStyles.body.copyWith(
                          fontWeight: FontWeight.bold,
                        ),
                      ),
                    ],
                    if (alert.text.isNotEmpty) ...[
                      const SizedBox(height: 10),
                      Text(
                        alert.text,
                        textAlign: TextAlign.center,
                        maxLines: 4,
                        overflow: TextOverflow.ellipsis,
                        style: AppTextStyles.small.copyWith(
                          color: theme.colorScheme.onSurface,
                        ),
                      ),
                    ],
                  ],
                ),
              ),
            ),
          ),
        ),
      ),
    );
  }
}
