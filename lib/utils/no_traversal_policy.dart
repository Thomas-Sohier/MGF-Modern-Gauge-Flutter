/// Politique de navigation clavier bloquée (aucun déplacement possible)
/// Une politique de focus qui implémente deux comportements distincts
/// basés sur un état d'édition.
library;

import 'package:flutter/widgets.dart';

class NoTraversalPolicy extends FocusTraversalPolicy {
  /// La politique par défaut à laquelle on délègue quand on n'est pas en mode édition.
  final FocusTraversalPolicy _defaultPolicy = WidgetOrderTraversalPolicy();

  /// Crée une politique de focus qui se comporte différemment en mode édition.
  NoTraversalPolicy();

  // --- Méthodes pour la navigation par Tab ---
  @override
  bool next(FocusNode currentNode) {
    return false;
  }

  @override
  bool previous(FocusNode currentNode) {
    return false;
  }

  // --- Méthodes pour la navigation directionnelle (flèches) ---

  @override
  bool inDirection(FocusNode currentNode, TraversalDirection direction) {
    return false;
  }

  // --- Méthodes pour trouver le premier/dernier focus ---

  @override
  FocusNode findFirstFocus(FocusNode currentNode, {bool ignoreCurrentFocus = true}) {
    return currentNode;
  }

  @override
  FocusNode findLastFocus(FocusNode currentNode, {bool ignoreCurrentFocus = true}) {
    return currentNode;
  }

  // --- NOUVELLES MÉTHODES REQUISES ---

  @override
  FocusNode? findFirstFocusInDirection(FocusNode currentNode, TraversalDirection direction) {
    return null;
  }

  @override
  Iterable<FocusNode> sortDescendants(Iterable<FocusNode> descendants, FocusNode currentNode) {
    return _defaultPolicy.sortDescendants(descendants, currentNode);
  }
}
