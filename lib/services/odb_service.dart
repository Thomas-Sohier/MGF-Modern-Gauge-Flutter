import 'dart:async';

import 'package:modern_gauge_flutter/models/dial_data.dart';
import 'package:modern_gauge_flutter/models/ecu_data.dart';
import 'package:modern_gauge_flutter/providers/app_state_provider.dart';
import 'package:modern_gauge_flutter/services/ecu_service.dart';
import 'package:modern_gauge_flutter/services/log_service.dart';

class OdbService {
  final EcuService _ecuService;
  final _controller = StreamController<DialData>.broadcast();
  final _statusController = StreamController<OdbConnectionStatus>.broadcast();
  StreamSubscription<EcuData>? _ecuSubscription;
  bool _isServiceRunning = false;

  /// Le flux de données auquel les autres parties de l'app peuvent s'abonner.
  Stream<DialData> get dataStream => _controller.stream;
  Stream<OdbConnectionStatus> get statusStream => _statusController.stream;

  OdbService(this._ecuService);

  void startOdbDataStream() {
    if (_isServiceRunning) return;
    _isServiceRunning = true;

    // Listen to raw ECU data and parse it
    _ecuSubscription = _ecuService.dataStream.listen((ecuData) {
      _processEcuData(ecuData);
    });

    LogService.info('[OdbService] - initialized and listening to EcuService');
  }

  void stopOdbDataStream() {
    if (!_isServiceRunning) return;
    _ecuSubscription?.cancel();
    _ecuSubscription = null;
    _isServiceRunning = false;
    _statusController.add(OdbConnectionStatus.disconnected);
    LogService.info('[OdbService] - stopped');
  }

  void _processEcuData(EcuData ecuData) {
    // Update connection status
    _statusController.add(ecuData.connected ? OdbConnectionStatus.connected : OdbConnectionStatus.disconnected);

    if (ecuData.ecuData == null) return;

    final raw = ecuData.ecuData!;

    // Map Go agent keys to DialData fields
    // Assuming keys based on common ECU data or previous mock values
    final data = DialData(
      rpm: _toDouble(raw['RPM']),
      speed: _toDouble(raw['Speed']),
      coolantTemp: _toDouble(raw['CoolantTemp']),
      fuelLevel: _toDouble(raw['FuelLevel']),
      oilPressure: _toDouble(raw['OilPressure']),
      batteryVoltage: _toDouble(raw['BatteryVoltage']),
      odometer: _toDouble(raw['Odometer']),
    );

    _controller.add(data);
    LogService.debug('[OdbService] - parsed data: $data');
  }

  double _toDouble(dynamic value) {
    if (value == null) return 0.0;
    if (value is num) return value.toDouble();
    if (value is String) return double.tryParse(value) ?? 0.0;
    return 0.0;
  }

  void dispose() {
    stopOdbDataStream();
    _controller.close();
    _statusController.close();
    LogService.debug('[OdbService] - disposed');
  }
}
