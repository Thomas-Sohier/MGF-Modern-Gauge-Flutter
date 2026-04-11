import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/models/ecu_data.dart';
import 'package:modern_gauge_flutter/services/ecu_service.dart';

class EcuProvider with ChangeNotifier {
  final EcuService _ecuService;
  EcuInfos _currentData = EcuInfos.initial();
  bool _initialDataFetched = false;

  EcuInfos get currentData => _currentData;
  bool get initialDataFetched => _initialDataFetched;

  EcuProvider(this._ecuService) {
    // Listen to WebSocket stream for real-time updates (no connection attempt here)
    _ecuService.dataStream.listen((data) {
      _currentData = data;
      notifyListeners();
    });
  }

  /// Connects the WebSocket and fetches initial data.
  /// Call this explicitly from the UI when ready.
  Future<void> connect() async {
    _ecuService.connectWebSocket();
    await fetchInitialData();
  }

  /// Fetches initial data from the ECU. Only executes once.
  /// Use [retryInitialData] to force a new fetch.
  Future<void> fetchInitialData() async {
    if (_initialDataFetched) return;

    _ecuService.fetchInitialData().then((initialData) {
      if (initialData != null) {
        _currentData = initialData;
        _initialDataFetched = true;
        notifyListeners();
      }
    });
  }

  /// Manually retries: reconnects the WebSocket and re-fetches initial data.
  Future<void> retryInitialData() async {
    _initialDataFetched = false;
    _ecuService.reconnectWebSocket();
    await fetchInitialData();
  }

  // --- Actions ---

  Future<void> setEcuType(String name) async {
    await _ecuService.setEcuType(name);
  }

  Future<void> setSerialPort(String name) async {
    await _ecuService.setSerialPort(name);
  }

  Future<void> sendCommand(String command) async {
    await _ecuService.sendCommand(command);
  }

  @override
  void dispose() {
    _ecuService.dispose();
    super.dispose();
  }
}
