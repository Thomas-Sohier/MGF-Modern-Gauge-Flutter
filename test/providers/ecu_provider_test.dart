import 'dart:async';
import 'package:flutter/scheduler.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:modern_gauge_flutter/models/ecu_data.dart';
import 'package:modern_gauge_flutter/providers/ecu_provider.dart';
import 'package:modern_gauge_flutter/services/ecu_service.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  group('EcuProvider', () {
    late FakeEcuService fakeService;
    EcuProvider? provider;

    setUp(() {
      fakeService = FakeEcuService();
    });

    tearDown(() {
      provider?.dispose();
      provider = null;
    });

    test('initializes with EcuInfos.initial()', () {
      provider = EcuProvider(fakeService);

      expect(provider!.currentData.connected, isFalse);
      expect(provider!.initialDataFetched, isFalse);
    });

    test('connects WebSocket on construction', () {
      provider = EcuProvider(fakeService);

      expect(fakeService.connectWebSocketCalled, isTrue);
    });

    test('updates currentData when service emits data', () async {
      provider = EcuProvider(fakeService);

      final testData = EcuInfos(
        connected: true,
        ecuType: 'MEMS',
        ecuData: EcuData.fromJson({'rpm': 4500}),
      );
      fakeService.emitData(testData);

      await Future.delayed(Duration.zero);
      _pumpFrame();

      expect(provider!.currentData.connected, isTrue);
      expect(provider!.currentData.ecuType, equals('MEMS'));
      expect(provider!.currentData.data.rpmValue, equals(4500));
    });

    test('fetchInitialData sets initialDataFetched on success', () async {
      fakeService.initialDataToReturn = EcuInfos(connected: true);
      provider = EcuProvider(fakeService);

      await Future.delayed(Duration.zero);

      expect(provider!.initialDataFetched, isTrue);
      expect(provider!.currentData.connected, isTrue);
    });

    test('fetchInitialData does not set flag on null response', () async {
      fakeService.initialDataToReturn = null;
      provider = EcuProvider(fakeService);

      await Future.delayed(Duration.zero);

      expect(provider!.initialDataFetched, isFalse);
    });

    test('retryInitialData reconnects and refetches', () async {
      fakeService.initialDataToReturn = null;
      provider = EcuProvider(fakeService);
      await Future.delayed(Duration.zero);

      expect(provider!.initialDataFetched, isFalse);
      expect(fakeService.reconnectWebSocketCalled, isFalse);

      fakeService.initialDataToReturn = EcuInfos(connected: true, ecuType: 'RETRY');
      await provider!.retryInitialData();

      expect(fakeService.reconnectWebSocketCalled, isTrue);
      expect(provider!.initialDataFetched, isTrue);
      expect(provider!.currentData.ecuType, equals('RETRY'));
    });

    test('setEcuType delegates to service', () async {
      provider = EcuProvider(fakeService);
      await provider!.setEcuType('MEMS');

      expect(fakeService.lastEcuType, equals('MEMS'));
    });

    test('setSerialPort delegates to service', () async {
      provider = EcuProvider(fakeService);
      await provider!.setSerialPort('/dev/ttyUSB0');

      expect(fakeService.lastSerialPort, equals('/dev/ttyUSB0'));
    });

    test('sendCommand delegates to service', () async {
      provider = EcuProvider(fakeService);
      await provider!.sendCommand('read_sensors');

      expect(fakeService.lastCommand, equals('read_sensors'));
    });

    test('clearFaults sends clear_faults command', () async {
      provider = EcuProvider(fakeService);
      await provider!.clearFaults();

      expect(fakeService.lastCommand, equals('clear_faults'));
    });

    test('dispose cancels subscription and disposes service', () {
      final localService = FakeEcuService();
      final localProvider = EcuProvider(localService);
      localProvider.dispose();

      expect(localService.disposed, isTrue);
    });

  });
}

void _pumpFrame() {
  final binding = SchedulerBinding.instance;
  binding.handleBeginFrame(Duration.zero);
  binding.handleDrawFrame();
}

class FakeEcuService implements EcuService {
  final _dataController = StreamController<EcuInfos>.broadcast();
  final _connectionController = StreamController<bool>.broadcast();

  bool connectWebSocketCalled = false;
  bool reconnectWebSocketCalled = false;
  bool disposed = false;

  String? lastEcuType;
  String? lastSerialPort;
  String? lastCommand;

  EcuInfos? initialDataToReturn;

  @override
  Stream<EcuInfos> get dataStream => _dataController.stream;

  @override
  Stream<bool> get connectionStream => _connectionController.stream;

  @override
  String get baseUrl => 'http://localhost:8080';

  @override
  String get wsUrl => 'ws://localhost:8080/ws';

  @override
  bool get isConnected => false;

  void emitData(EcuInfos data) => _dataController.add(data);

  @override
  void connectWebSocket() {
    connectWebSocketCalled = true;
  }

  @override
  void reconnectWebSocket() {
    reconnectWebSocketCalled = true;
  }

  @override
  Future<EcuInfos?> fetchInitialData() async => initialDataToReturn;

  @override
  Future<bool> ping() async => true;

  @override
  Future<void> setEcuType(String name) async {
    lastEcuType = name;
  }

  @override
  Future<void> setSerialPort(String name) async {
    lastSerialPort = name;
  }

  @override
  Future<void> sendCommand(String command) async {
    lastCommand = command;
  }

  @override
  void dispose() {
    disposed = true;
    _dataController.close();
    _connectionController.close();
  }
}
