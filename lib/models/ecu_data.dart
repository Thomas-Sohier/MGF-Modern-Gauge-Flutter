class EcuData {
  final Map<String, dynamic>? faults;
  final bool connected;
  final String? ecuType;
  final String? userCommand;
  final String? alert;
  final String? error;
  final Map<String, dynamic>? ecuData;
  final String? agentVersion;
  final String? timestamp;
  final List<String>? serialPorts;
  final String? selectedSerialPort;
  final List<String>? logLines;

  EcuData({
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

  factory EcuData.fromJson(Map<String, dynamic> json) {
    return EcuData(
      faults: json['faults'] as Map<String, dynamic>?,
      connected: json['connected'] ?? false,
      ecuType: json['ecuType'] as String?,
      userCommand: json['userCommand'] as String?,
      alert: json['alert'] as String?,
      error: json['error'] as String?,
      ecuData: json['ecuData'] as Map<String, dynamic>?,
      agentVersion: json['agentVersion'] as String?,
      timestamp: json['timestamp'] as String?,
      serialPorts: (json['serialPorts'] as List<dynamic>?)?.map((e) => e.toString()).toList(),
      selectedSerialPort: json['selectedSerialPort'] as String?,
      logLines: (json['logLines'] as List<dynamic>?)?.map((e) => e.toString()).toList(),
    );
  }

  factory EcuData.initial() {
    return EcuData(connected: false, ecuData: {});
  }
}
