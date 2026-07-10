import 'package:flutter/material.dart';

/// Couleurs de l'écran lecteur de musique (design MD3 "Écran rond — lecteur").
@immutable
class MusicPlayerTheme extends ThemeExtension<MusicPlayerTheme> {
  const MusicPlayerTheme({
    required this.surface,
    required this.primary,
    required this.ringTrack,
    required this.title,
    required this.subtle,
    required this.artFallback,
  });

  /// Fond de l'écran et couleur de l'icône du bouton play/pause.
  final Color surface;

  /// Progression de l'anneau et fond du bouton play/pause.
  final Color primary;

  /// Piste inactive de l'anneau de progression.
  final Color ringTrack;

  /// Titre du morceau.
  final Color title;

  /// Artiste, temps, icône de pochette par défaut.
  final Color subtle;

  /// Fond de la pochette quand aucune image n'est disponible.
  final Color artFallback;

  @override
  MusicPlayerTheme copyWith({
    Color? surface,
    Color? primary,
    Color? ringTrack,
    Color? title,
    Color? subtle,
    Color? artFallback,
  }) {
    return MusicPlayerTheme(
      surface: surface ?? this.surface,
      primary: primary ?? this.primary,
      ringTrack: ringTrack ?? this.ringTrack,
      title: title ?? this.title,
      subtle: subtle ?? this.subtle,
      artFallback: artFallback ?? this.artFallback,
    );
  }

  @override
  MusicPlayerTheme lerp(ThemeExtension<MusicPlayerTheme>? other, double t) {
    if (other is! MusicPlayerTheme) {
      return this;
    }
    return MusicPlayerTheme(
      surface: Color.lerp(surface, other.surface, t)!,
      primary: Color.lerp(primary, other.primary, t)!,
      ringTrack: Color.lerp(ringTrack, other.ringTrack, t)!,
      title: Color.lerp(title, other.title, t)!,
      subtle: Color.lerp(subtle, other.subtle, t)!,
      artFallback: Color.lerp(artFallback, other.artFallback, t)!,
    );
  }
}
