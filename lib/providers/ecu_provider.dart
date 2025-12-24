import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/models/ecu_data.dart';
import 'package:modern_gauge_flutter/services/ecu_service.dart';

class EcuProvider with ChangeNotifier {
  final EcuService _ecuService;
  EcuData _currentData = EcuData.initial();

  EcuData get currentData => _currentData;

  EcuProvider(this._ecuService) {
    _init();
  }

  Future<void> _init() async {
    // Initial fetch
    final initialData = await _ecuService.fetchInitialData();
    if (initialData != null) {
      _currentData = initialData;
      notifyListeners();
    }

    // Connect to WebSocket for real-time updates
    _ecuService.dataStream.listen((data) {
      _currentData = data;
      notifyListeners();
    });

    _ecuService.connectWebSocket();
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
