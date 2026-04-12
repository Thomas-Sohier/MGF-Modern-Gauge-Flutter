import 'package:flutter/material.dart';

enum OdbConnectionStatus { disconnected, connecting, connected, error }

class AppStateProvider with ChangeNotifier {
  // Vrai si l'application est en mode veille
  bool _isAsleep = false;
  // Vrai pendant le démarrage initial de l'app
  bool _isInitializing = true;
  // Message d'erreur quui prend le dessus sur l'app
  String? _globalAlertMessage;

  bool get isAsleep => _isAsleep;
  bool get isInitializing => _isInitializing;
  String? get globalAlertMessage => _globalAlertMessage;

  AppStateProvider();

  void setSleepMode(bool asleep) {
    if (_isAsleep != asleep) {
      _isAsleep = asleep;
      notifyListeners();
    }
  }

  void finishInitialization() {
    if (_isInitializing) {
      _isInitializing = false;
      notifyListeners();
    }
  }

  void setGlobalAlert(String? message) {
    _globalAlertMessage = message;
    notifyListeners();
  }
}
