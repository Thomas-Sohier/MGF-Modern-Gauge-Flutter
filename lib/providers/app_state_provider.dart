import 'dart:async';

import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/services/odb_service.dart';

enum OdbConnectionStatus { disconnected, connecting, connected, error }

class AppStateProvider with ChangeNotifier {
  final OdbService _odbService;
  late final StreamSubscription<OdbConnectionStatus> _statusSubscription;
  // Vrai si l'application est en mode veille
  bool _isAsleep = false;
  // État de la connexion ODB
  OdbConnectionStatus _odbStatus = OdbConnectionStatus.disconnected;
  // Vrai pendant le démarrage initial de l'app
  bool _isInitializing = true;
  // Message d'erreur quui prend le dessus sur l'app
  String? _globalAlertMessage;

  bool get isAsleep => _isAsleep;
  OdbConnectionStatus get odbStatus => _odbStatus;
  bool get isInitializing => _isInitializing;
  String? get globalAlertMessage => _globalAlertMessage;

  AppStateProvider(this._odbService) {
    _statusSubscription = _odbService.statusStream.listen((newStatus) {
      setOdbConnectionStatus(newStatus);
    });
  }

  void setSleepMode(bool asleep) {
    if (_isAsleep != asleep) {
      _isAsleep = asleep;
      notifyListeners();
    }
  }

  void setOdbConnectionStatus(OdbConnectionStatus status) {
    if (_odbStatus != status) {
      _odbStatus = status;
      notifyListeners();
      // Potentiellement gérer des erreurs ou des reconnexions ici
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

  @override
  void dispose() {
    _statusSubscription.cancel();
    super.dispose();
  }
}
