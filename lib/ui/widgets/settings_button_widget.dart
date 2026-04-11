import 'package:flutter/material.dart';

/// A customizable circular button widget typically used in settings screens.
class SettingsButtonWidget extends StatelessWidget {
  /// The text displayed inside the button.
  final String text;

  /// The icon displayed above the text (optional).
  final IconData? icon;

  /// Callback function when the button is pressed.
  final VoidCallback onPressed;

  /// The background color of the button.
  final Color backgroundColor;

  /// The color of the text and icon.
  final Color foregroundColor;

  /// The size of the icon.
  final double iconSize;

  /// The font size of the text.
  final double textSize;

  /// Constructor for SettingsButtonWidget.
  const SettingsButtonWidget({
    super.key,
    required this.text,
    this.icon,
    required this.onPressed,
    this.backgroundColor = Colors.grey, // Default background
    this.foregroundColor = Colors.white, // Default foreground
    this.iconSize = 28.0,
    this.textSize = 14.0,
  });

  @override
  Widget build(BuildContext context) {
    return InkWell(
      // Use InkWell for custom tap effects on a custom shape
      onTap: onPressed,
      borderRadius: BorderRadius.circular(
        100,
      ), // Makes the ripple effect circular
      child: Container(
        width: 100, // Fixed width for the circular button
        height: 100, // Fixed height for the circular button
        decoration: BoxDecoration(
          color: backgroundColor,
          shape: BoxShape.circle, // Make it circular
          boxShadow: [
            BoxShadow(
              color: Colors.black.withOpacity(0.2),
              blurRadius: 5,
              offset: const Offset(0, 3),
            ),
          ],
        ),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            if (icon != null) ...[
              Icon(icon, color: foregroundColor, size: iconSize),
              const SizedBox(height: 5),
            ],
            Text(
              text,
              textAlign: TextAlign.center,
              style: TextStyle(
                color: foregroundColor,
                fontSize: textSize,
                fontWeight: FontWeight.w600,
              ),
            ),
          ],
        ),
      ),
    );
  }
}
