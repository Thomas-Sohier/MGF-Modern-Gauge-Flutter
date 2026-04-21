class EcuInfos {
  final List<dynamic>? faults;
  final bool connected;
  final String? ecuType;
  final String? userCommand;
  final String? alert;
  final String? error;
  final EcuData? ecuData;
  final String? agentVersion;
  final String? timestamp;
  final List<String>? serialPorts;
  final String? selectedSerialPort;
  final List<String>? logLines;

  EcuInfos({
    this.faults,
    this.connected = false,
    this.ecuType,
    this.userCommand,
    this.alert,
    this.error,
    this.ecuData,
    this.agentVersion,
    this.timestamp,
    this.serialPorts,
    this.selectedSerialPort,
    this.logLines,
  });

  factory EcuInfos.fromJson(Map<String, dynamic> json) {
    return EcuInfos(
      faults: json['faults'] as List<dynamic>?,
      connected: json['connected'] ?? false,
      ecuType: json['ecuType']?.toString(),
      userCommand: json['userCommand']?.toString(),
      alert: json['alert']?.toString(),
      error: json['error']?.toString(),
      ecuData: json['ecuData'] is Map
          ? EcuData.fromJson(Map<String, dynamic>.from(json['ecuData'] as Map))
          : null,
      agentVersion: json['agentVersion']?.toString(),
      timestamp: json['timestamp']?.toString(),
      serialPorts: (json['serialPorts'] as List<dynamic>?)
          ?.map((e) => e.toString())
          .toList(),
      selectedSerialPort: json['selectedSerialPort']?.toString(),
      logLines: (json['logLines'] as List<dynamic>?)
          ?.map((e) => e.toString())
          .toList(),
    );
  }

  factory EcuInfos.initial() {
    return EcuInfos(connected: false, ecuData: EcuData.initial());
  }
}

/// ECU sensor data backed by raw JSON map.
///
/// Lazy getters cast on access — avoids allocating 53 fields per fromJson call
/// at 10 Hz. Only fields actually read by the UI incur the cast cost.
class EcuData {
  final Map<String, dynamic> _json;

  const EcuData._(this._json);

  factory EcuData.fromJson(Map<String, dynamic> json) => EcuData._(json);

  factory EcuData.initial() => const EcuData._({});

  // Typed getters — cast on access
  num? get acButton => _json['ac_button'] as num?;
  num? get ambientTemp => _json['ambient_temp'] as num?;
  num? get batteryVoltage => _json['battery_voltage'] as num?;
  num? get camPercent => _json['cam_percent'] as num?;
  num? get carbonCanPurgeValveDutyCycle => _json['carbon_can_purge_valve_duty_cycle'] as num?;
  num? get closedLoop => _json['closed_loop'] as num?;
  num? get coil1ChargeTime => _json['coil_1_charge_time'] as num?;
  num? get coil2ChargeTime => _json['coil_2_charge_time'] as num?;
  num? get coilTimeMicroseconds => _json['coil_time_microseconds'] as num?;
  num? get coolantTemp => _json['coolant_temp'] as num?;
  num? get crankCounter => _json['crank_counter'] as num?;
  num? get estimateAirFuel => _json['estimate_air_fuel'] as num?;
  num? get fan1Control => _json['fan_1_control'] as num?;
  num? get fan2Control => _json['fan_2_control'] as num?;
  num? get fuelRailTemp => _json['fuel_rail_temp'] as num?;
  num? get fuellingFeedbackPercent => _json['fuelling_feedback_percent'] as num?;
  num? get idleAdjusterRpm => _json['idle_adjuster_rpm'] as num?;
  num? get idleBasePosition => _json['idle_base_position'] as num?;
  num? get idleError => _json['idle_error'] as num?;
  num? get idleSetpoint => _json['idle_setpoint'] as num?;
  num? get idleSpeedDeviation => _json['idle_speed_deviation'] as num?;
  num? get idleSwitch => _json['idle_switch'] as num?;
  num? get idleTimingOffset => _json['idle_timing_offset'] as num?;
  num? get idleValvePosition => _json['idle_valve_position'] as num?;
  num? get ignition => _json['ignition'] as num?;
  num? get ignitionAdvance => _json['ignition_advance'] as num?;
  num? get ignitionAdvanceOffset => _json['ignition_advance_offset'] as num?;
  num? get ignitionSwitch => _json['ignition_switch'] as num?;
  num? get injector14Driver => _json['injector_1_4_driver'] as num?;
  num? get injector1Pw => _json['injector_1_pw'] as num?;
  num? get injector23Driver => _json['injector_2_3_driver'] as num?;
  num? get injector2Pw => _json['injector_2_pw'] as num?;
  num? get injector3Pw => _json['injector_3_pw'] as num?;
  num? get injector4Pw => _json['injector_4_pw'] as num?;
  num? get intakeAirTemp => _json['intake_air_temp'] as num?;
  num? get lambdaHeaterRelay => _json['lambda_heater_relay'] as num?;
  num? get lambdaMv => _json['lambda_mv'] as num?;
  num? get lambdaSensorDutyCycle => _json['lambda_sensor_duty_cycle'] as num?;
  num? get lambdaSensorFrequency => _json['lambda_sensor_frequency'] as num?;
  num? get lambdaSensorStatus => _json['lambda_sensor_status'] as num?;
  num? get longTermTrim => _json['long_term_trim'] as num?;
  num? get mapSensorKpa => _json['map_sensor_kpa'] as num?;
  num? get o2Mv => _json['o2_mv'] as num?;
  num? get oilTemp => _json['oil_temp'] as num?;
  num? get parkOrNeutralSwitch => _json['park_or_neutral_switch'] as num?;
  num? get primaryTriggerSync => _json['primary_trigger_sync'] as num?;
  num? get rpm => _json['rpm'] as num?;
  num? get rpmError => _json['rpm_error'] as num?;
  num? get secondaryTriggerSync => _json['secondary_trigger_sync'] as num?;
  num? get shortTermTrimPercent => _json['short_term_trim_percent'] as num?;
  num? get throttleAngle => _json['throttle_angle'] as num?;
  num? get throttlePotVoltage => _json['throttle_pot_voltage'] as num?;
  num? get throttleSwitch => _json['throttle_switch'] as num?;
  num? get vehicleSpeed => _json['vehicle_speed'] as num?;
}
