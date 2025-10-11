import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/ui/widgets/gauge_background.dart';

class DashboardShellScreen extends StatelessWidget {
  final Widget child;

  const DashboardShellScreen({required this.child, super.key});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: Colors.transparent,
      body: Center(child: GaugeTexturedBackground(child: child)),
    );
  }
}
