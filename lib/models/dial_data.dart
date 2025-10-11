class DialData {
  final double rpm;

  /// par exemple en km/h
  final double speed;

  /// température du liquide de refroidissement en °C
  final double coolantTemp;

  /// niveau de carburant en pourcentage
  final double fuelLevel;

  /// pression d'huile
  final double oilPressure;

  /// voltage de la batterie
  final double batteryVoltage;

  /// kilométrage total
  final double odometer;

  DialData({
    this.rpm = 0.0,
    this.speed = 0.0,
    this.coolantTemp = 0.0,
    this.fuelLevel = 0.0,
    this.oilPressure = 0.0,
    this.batteryVoltage = 0.0,
    this.odometer = 0.0,
  });

  DialData copyWith({
    double? rpm,
    double? speed,
    double? coolantTemp,
    double? fuelLevel,
    double? oilPressure,
    double? batteryVoltage,
    double? odometer,
  }) {
    return DialData(
      rpm: rpm ?? this.rpm,
      speed: speed ?? this.speed,
      coolantTemp: coolantTemp ?? this.coolantTemp,
      fuelLevel: fuelLevel ?? this.fuelLevel,
      oilPressure: oilPressure ?? this.oilPressure,
      batteryVoltage: batteryVoltage ?? this.batteryVoltage,
      odometer: odometer ?? this.odometer,
    );
  }

  @override
  String toString() {
    return 'DialData(rpm: $rpm, speed: $speed, coolantTemp: $coolantTemp, fuelLevel: $fuelLevel, oilPressure: $oilPressure, batteryVoltage: $batteryVoltage, odometer: $odometer)';
  }
}
