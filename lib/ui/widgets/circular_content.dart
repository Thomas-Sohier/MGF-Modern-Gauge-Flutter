import 'package:flutter/material.dart';

/// Constrains [child] to the largest centred circle that fits the available
/// space (a square clipped to an oval).
///
/// Use this to keep screen content inside the round physical display while the
/// background is free to fill the whole framebuffer. The child receives tight
/// square constraints, so layouts can rely on a 1:1 box.
class CircularContent extends StatelessWidget {
  final Widget child;

  const CircularContent({super.key, required this.child});

  @override
  Widget build(BuildContext context) {
    return Center(
      child: AspectRatio(
        aspectRatio: 1,
        child: ClipOval(child: child),
      ),
    );
  }
}
