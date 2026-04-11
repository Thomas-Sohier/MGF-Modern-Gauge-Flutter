import 'package:flutter/material.dart';

/// Centralised JetBrainsMono text styles for the whole app.
///
/// Sizes are tuned for readability on a 4" screen (minimum 16px).
/// Styles without a color inherit from the ambient [DefaultTextStyle].
/// Styles that require a dynamic color (gauge value, unit) take a [Color] parameter.
class AppTextStyles {
  static const String _font = 'JetBrainsMono';

  /// Large gauge reading — e.g. RPM value (45px bold).
  static TextStyle display(Color color) => TextStyle(
        fontFamily: _font,
        fontSize: 45,
        fontWeight: FontWeight.bold,
        color: color,
      );

  /// Unit label beside a gauge value — e.g. "RPM", "km/h" (20px w500).
  static TextStyle unit(Color color) => TextStyle(
        fontFamily: _font,
        fontSize: 20,
        fontWeight: FontWeight.w500,
        color: color,
      );

  /// Screen or section title — e.g. "Codes erreurs" (20px w700).
  static const TextStyle title = TextStyle(
    fontFamily: _font,
    fontSize: 20,
    fontWeight: FontWeight.w700,
  );

  /// Standard body text — e.g. music timestamps, empty-state messages (18px w500).
  static const TextStyle body = TextStyle(
    fontFamily: _font,
    fontSize: 18,
    fontWeight: FontWeight.w500,
  );

  /// Bold label — e.g. indicator values below gauges (18px w700).
  static const TextStyle label = TextStyle(
    fontFamily: _font,
    fontSize: 18,
    fontWeight: FontWeight.w700,
  );

  /// Small readable text — e.g. fault code list items (16px w500).
  static const TextStyle small = TextStyle(
    fontFamily: _font,
    fontSize: 16,
    fontWeight: FontWeight.w500,
  );
}
