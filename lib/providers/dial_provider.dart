import 'dart:async';

import 'package:flutter/material.dart';
import 'package:modern_gauge_flutter/models/dial_data.dart';
import 'package:modern_gauge_flutter/services/odb_service.dart';

class DialProvider with ChangeNotifier {
  final OdbService _odbService;
  late final StreamSubscription<DialData> _dataSubscription;
  DialData _dialData = DialData();

  DialData get dialData => _dialData;

  DialProvider(this._odbService) {
    _dataSubscription = _odbService.dataStream.listen((newData) {
      updateAllDialData(newData);
    });
  }

  void updateAllDialData(DialData newData) {
    _dialData = newData;
    notifyListeners();
  }

  void updateRpm(double rpm) {
    _dialData = _dialData.copyWith(rpm: rpm);
    notifyListeners();
  }

  void updateSpeed(double speed) {
    _dialData = _dialData.copyWith(speed: speed);
    notifyListeners();
  }

  void updateCoolantTemp(double temp) {
    _dialData = _dialData.copyWith(coolantTemp: temp);
    notifyListeners();
  }

  void updateFuelLevel(double level) {
    _dialData = _dialData.copyWith(fuelLevel: level);
    notifyListeners();
  }

  void updateOilPressure(double pressure) {
    _dialData = _dialData.copyWith(oilPressure: pressure);
    notifyListeners();
  }

  void updateBatteryVoltage(double voltage) {
    _dialData = _dialData.copyWith(batteryVoltage: voltage);
    notifyListeners();
  }

  void updateOdometer(double odometer) {
    _dialData = _dialData.copyWith(odometer: odometer);
    notifyListeners();
  }

  void resetDialData() {
    _dialData = DialData();
    notifyListeners();
  }

  @override
  void dispose() {
    _dataSubscription.cancel();
    super.dispose();
  }
}
