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

class EcuData {
  final num? acButton;
  final num? ambientTemp;
  final num? batteryVoltage;
  final num? camPercent;
  final num? carbonCanPurgeValveDutyCycle;
  final num? closedLoop;
  final num? coil1ChargeTime;
  final num? coil2ChargeTime;
  final num? coilTimeMicroseconds;
  final num? coolantTemp;
  final num? crankCounter;
  final num? estimateAirFuel;
  final num? fan1Control;
  final num? fan2Control;
  final num? fuelRailTemp;
  final num? fuellingFeedbackPercent;
  final num? idleAdjusterRpm;
  final num? idleBasePosition;
  final num? idleError;
  final num? idleSetpoint;
  final num? idleSpeedDeviation;
  final num? idleSwitch;
  final num? idleTimingOffset;
  final num? idleValvePosition;
  final num? ignition;
  final num? ignitionAdvance;
  final num? ignitionAdvanceOffset;
  final num? ignitionSwitch;
  final num? injector14Driver;
  final num? injector1Pw;
  final num? injector23Driver;
  final num? injector2Pw;
  final num? injector3Pw;
  final num? injector4Pw;
  final num? intakeAirTemp;
  final num? lambdaHeaterRelay;
  final num? lambdaMv;
  final num? lambdaSensorDutyCycle;
  final num? lambdaSensorFrequency;
  final num? lambdaSensorStatus;
  final num? longTermTrim;
  final num? mapSensorKpa;
  final num? o2Mv;
  final num? oilTemp;
  final num? parkOrNeutralSwitch;
  final num? primaryTriggerSync;
  final num? rpm;
  final num? rpmError;
  final num? secondaryTriggerSync;
  final num? shortTermTrimPercent;
  final num? throttleAngle;
  final num? throttlePotVoltage;
  final num? throttleSwitch;
  final num? vehicleSpeed;

  EcuData({
    this.acButton,
    this.ambientTemp,
    this.batteryVoltage,
    this.camPercent,
    this.carbonCanPurgeValveDutyCycle,
    this.closedLoop,
    this.coil1ChargeTime,
    this.coil2ChargeTime,
    this.coilTimeMicroseconds,
    this.coolantTemp,
    this.crankCounter,
    this.estimateAirFuel,
    this.fan1Control,
    this.fan2Control,
    this.fuelRailTemp,
    this.fuellingFeedbackPercent,
    this.idleAdjusterRpm,
    this.idleBasePosition,
    this.idleError,
    this.idleSetpoint,
    this.idleSpeedDeviation,
    this.idleSwitch,
    this.idleTimingOffset,
    this.idleValvePosition,
    this.ignition,
    this.ignitionAdvance,
    this.ignitionAdvanceOffset,
    this.ignitionSwitch,
    this.injector14Driver,
    this.injector1Pw,
    this.injector23Driver,
    this.injector2Pw,
    this.injector3Pw,
    this.injector4Pw,
    this.intakeAirTemp,
    this.lambdaHeaterRelay,
    this.lambdaMv,
    this.lambdaSensorDutyCycle,
    this.lambdaSensorFrequency,
    this.lambdaSensorStatus,
    this.longTermTrim,
    this.mapSensorKpa,
    this.o2Mv,
    this.oilTemp,
    this.parkOrNeutralSwitch,
    this.primaryTriggerSync,
    this.rpm,
    this.rpmError,
    this.secondaryTriggerSync,
    this.shortTermTrimPercent,
    this.throttleAngle,
    this.throttlePotVoltage,
    this.throttleSwitch,
    this.vehicleSpeed,
  });

  factory EcuData.fromJson(Map<String, dynamic> json) {
    return EcuData(
      acButton: json['ac_button'] as num?,
      ambientTemp: json['ambient_temp'] as num?,
      batteryVoltage: json['battery_voltage'] as num?,
      camPercent: json['cam_percent'] as num?,
      carbonCanPurgeValveDutyCycle: json['carbon_can_purge_valve_duty_cycle'] as num?,
      closedLoop: json['closed_loop'] as num?,
      coil1ChargeTime: json['coil_1_charge_time'] as num?,
      coil2ChargeTime: json['coil_2_charge_time'] as num?,
      coilTimeMicroseconds: json['coil_time_microseconds'] as num?,
      coolantTemp: json['coolant_temp'] as num?,
      crankCounter: json['crank_counter'] as num?,
      estimateAirFuel: json['estimate_air_fuel'] as num?,
      fan1Control: json['fan_1_control'] as num?,
      fan2Control: json['fan_2_control'] as num?,
      fuelRailTemp: json['fuel_rail_temp'] as num?,
      fuellingFeedbackPercent: json['fuelling_feedback_percent'] as num?,
      idleAdjusterRpm: json['idle_adjuster_rpm'] as num?,
      idleBasePosition: json['idle_base_position'] as num?,
      idleError: json['idle_error'] as num?,
      idleSetpoint: json['idle_setpoint'] as num?,
      idleSpeedDeviation: json['idle_speed_deviation'] as num?,
      idleSwitch: json['idle_switch'] as num?,
      idleTimingOffset: json['idle_timing_offset'] as num?,
      idleValvePosition: json['idle_valve_position'] as num?,
      ignition: json['ignition'] as num?,
      ignitionAdvance: json['ignition_advance'] as num?,
      ignitionAdvanceOffset: json['ignition_advance_offset'] as num?,
      ignitionSwitch: json['ignition_switch'] as num?,
      injector14Driver: json['injector_1_4_driver'] as num?,
      injector1Pw: json['injector_1_pw'] as num?,
      injector23Driver: json['injector_2_3_driver'] as num?,
      injector2Pw: json['injector_2_pw'] as num?,
      injector3Pw: json['injector_3_pw'] as num?,
      injector4Pw: json['injector_4_pw'] as num?,
      intakeAirTemp: json['intake_air_temp'] as num?,
      lambdaHeaterRelay: json['lambda_heater_relay'] as num?,
      lambdaMv: json['lambda_mv'] as num?,
      lambdaSensorDutyCycle: json['lambda_sensor_duty_cycle'] as num?,
      lambdaSensorFrequency: json['lambda_sensor_frequency'] as num?,
      lambdaSensorStatus: json['lambda_sensor_status'] as num?,
      longTermTrim: json['long_term_trim'] as num?,
      mapSensorKpa: json['map_sensor_kpa'] as num?,
      o2Mv: json['o2_mv'] as num?,
      oilTemp: json['oil_temp'] as num?,
      parkOrNeutralSwitch: json['park_or_neutral_switch'] as num?,
      primaryTriggerSync: json['primary_trigger_sync'] as num?,
      rpm: json['rpm'] as num?,
      rpmError: json['rpm_error'] as num?,
      secondaryTriggerSync: json['secondary_trigger_sync'] as num?,
      shortTermTrimPercent: json['short_term_trim_percent'] as num?,
      throttleAngle: json['throttle_angle'] as num?,
      throttlePotVoltage: json['throttle_pot_voltage'] as num?,
      throttleSwitch: json['throttle_switch'] as num?,
      vehicleSpeed: json['vehicle_speed'] as num?,
    );
  }

  factory EcuData.initial() {
    return EcuData(); // all fields default to null
  }
}
