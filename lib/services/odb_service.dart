import 'dart:async';
import 'dart:math';

import 'package:flutter/foundation.dart';
import 'package:modern_gauge_flutter/models/dial_data.dart';
import 'package:modern_gauge_flutter/providers/app_state_provider.dart';
import 'package:modern_gauge_flutter/services/log_service.dart';

class OdbService {
  final _controller = StreamController<DialData>.broadcast();
  final _statusController = StreamController<OdbConnectionStatus>.broadcast();
  Timer? _dataFetchTimer;
  bool _isServiceRunning = false;

  /// Le flux de données auquel les autres parties de l'app peuvent s'abonner.
  Stream<DialData> get dataStream => _controller.stream;
  Stream<OdbConnectionStatus> get statusStream => _statusController.stream;

  void startOdbDataStream() {
    if (_isServiceRunning) return;
    _isServiceRunning = true;
    _statusController.add(OdbConnectionStatus.connected);
    _dataFetchTimer = Timer.periodic(const Duration(milliseconds: 400), (timer) {
      _readAndProcessOdbData();
    });
    LogService.info('[OdbService] - initialized');
  }

  void stopOdbDataStream() {
    if (!_isServiceRunning) return;
    _dataFetchTimer?.cancel();
    _isServiceRunning = false;
    _statusController.add(OdbConnectionStatus.disconnected);
    LogService.info('[OdbService] - stopped');
  }

  void _readAndProcessOdbData() {
    final random = Random();
    final newRpm = (random.nextDouble() * 5000).clamp(0.0, 7000.0); // 0-7000 RPM
    final newSpeed = (random.nextDouble() * 180).clamp(0.0, 220.0); // 0-220 km/h
    final newCoolantTemp = (random.nextDouble() * 80 + 20).clamp(0.0, 120.0); // 20-100°C
    final newFuelLevel = (random.nextDouble() * 100).clamp(0.0, 100.0); // 0-100%
    final newOilPressure = (random.nextDouble() * 5).clamp(0.0, 8.0); // 0-8 bars
    final newBatteryVoltage = (random.nextDouble() * 2 + 12).clamp(0.0, 15.0); // 12-14V
    final newOdometer = (random.nextDouble() * 100000).clamp(0.0, 999999.0); // Km aléatoires

    final data = DialData(
      rpm: newRpm,
      speed: newSpeed,
      coolantTemp: newCoolantTemp,
      fuelLevel: newFuelLevel,
      oilPressure: newOilPressure,
      batteryVoltage: newBatteryVoltage,
      odometer: newOdometer,
    );
    _controller.add(data);
    LogService.debug('[OdbService] - data: $data');
  }

  void dispose() {
    stopOdbDataStream();
    _controller.close();
    _statusController.close();
    LogService.debug('[OdbService] - disposed');
  }
}
