import 'dart:async';
import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/models/ecu_data.dart';
import 'package:modern_gauge_flutter/services/ecu_service.dart';

class EcuProvider with ChangeNotifier {
  final EcuService _ecuService;
  late final StreamSubscription<EcuInfos> _dataSubscription;

  /// Throttle notifyListeners() to ~6Hz max. Gauges don't need 10Hz updates —
  /// human perception can't distinguish above ~5-6Hz for numeric displays.
  static const _minNotifyInterval = Duration(milliseconds: 166);
  DateTime _lastNotify = DateTime(0);

  EcuInfos _currentData = EcuInfos.initial();
  bool _initialDataFetched = false;

  EcuInfos get currentData => _currentData;
  bool get initialDataFetched => _initialDataFetched;

  EcuProvider(this._ecuService) {
    _dataSubscription = _ecuService.dataStream.listen((data) {
      _currentData = data;
      final now = DateTime.now();
      if (now.difference(_lastNotify) >= _minNotifyInterval) {
        _lastNotify = now;
        notifyListeners();
      }
    });
    _ecuService.connectWebSocket();
    _fetchInitialData();
  }

  Future<void> _fetchInitialData() async {
    if (_initialDataFetched) return;
    final initialData = await _ecuService.fetchInitialData();
    if (initialData != null) {
      _currentData = initialData;
      _initialDataFetched = true;
      notifyListeners();
    }
  }

  /// Manually retries: reconnects the WebSocket and re-fetches initial data.
  Future<void> retryInitialData() async {
    _initialDataFetched = false;
    _ecuService.reconnectWebSocket();
    await _fetchInitialData();
  }

  // --- Actions ---

  Future<void> setEcuType(String name) => _ecuService.setEcuType(name);
  Future<void> setSerialPort(String name) => _ecuService.setSerialPort(name);
  Future<void> sendCommand(String command) => _ecuService.sendCommand(command);
  Future<void> clearFaults() => _ecuService.sendCommand('clear_faults');

  @override
  void dispose() {
    _dataSubscription.cancel();
    _ecuService.dispose();
    super.dispose();
  }
}
