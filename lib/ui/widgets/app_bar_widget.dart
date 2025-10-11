import 'package:flutter/material.dart';

/// A custom AppBar widget for consistent styling and common actions.
///
/// This widget can be used across different screens to maintain a uniform look
/// and provide a centralized place for AppBar-related logic or branding.
class CustomAppBar extends StatelessWidget implements PreferredSizeWidget {
  /// The title displayed in the AppBar.
  final String title;

  /// A list of widgets to display in a row after the [title] widget.
  final List<Widget>? actions;

  /// A widget to display before the [title] in the AppBar.
  /// Typically an [IconButton] or a [BackButton].
  final Widget? leading;

  /// The background color of the AppBar. Defaults to transparent.
  final Color backgroundColor;

  /// The color of the title text. Defaults to white.
  final Color titleColor;

  /// Whether the AppBar should cast a shadow. Defaults to 0 (no shadow).
  final double elevation;

  /// Constructor for CustomAppBar.
  const CustomAppBar({
    super.key,
    required this.title,
    this.actions,
    this.leading,
    this.backgroundColor = Colors.transparent,
    this.titleColor = Colors.white,
    this.elevation = 0.0,
  });

  @override
  Widget build(BuildContext context) {
    return AppBar(
      title: Text(title, style: TextStyle(color: titleColor)),
      leading: leading,
      actions: actions,
      backgroundColor: backgroundColor,
      elevation: elevation,
      iconTheme: IconThemeData(color: titleColor), // Ensure back button and other icons match title color
      centerTitle: true, // Often desired for custom app bars
    );
  }

  @override
  Size get preferredSize => const Size.fromHeight(kToolbarHeight); // Standard AppBar height
}
