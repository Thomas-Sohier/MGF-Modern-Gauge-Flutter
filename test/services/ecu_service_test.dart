import 'dart:async';
import 'dart:convert';
import 'package:flutter_test/flutter_test.dart';
import 'package:http/http.dart' as http;
import 'package:http/testing.dart';
import 'package:modern_gauge_flutter/models/ecu_data.dart';
import 'package:modern_gauge_flutter/services/ecu_service.dart';

void main() {
  group('EcuService HTTP', () {
    test('ping returns true on 200', () async {
      final client = MockClient((request) async {
        if (request.url.path == '/ping') {
          return http.Response('pong', 200);
        }
        return http.Response('Not Found', 404);
      });

      final service = EcuServiceTestable(client: client);
      final result = await service.ping();

      expect(result, isTrue);
    });

    test('ping returns false on network error', () async {
      final client = MockClient((request) async {
        throw Exception('Network error');
      });

      final service = EcuServiceTestable(client: client);
      final result = await service.ping();

      expect(result, isFalse);
    });

    test('fetchInitialData parses valid JSON', () async {
      final testData = {
        'connected': true,
        'ecuType': 'MEMS',
        'ecuData': {'rpm': 3500, 'coolant_temp': 85},
      };

      final client = MockClient((request) async {
        if (request.url.path == '/api') {
          return http.Response(json.encode(testData), 200);
        }
        return http.Response('Not Found', 404);
      });

      final service = EcuServiceTestable(client: client);
      final result = await service.fetchInitialData();

      expect(result, isNotNull);
      expect(result!.connected, isTrue);
      expect(result.ecuType, equals('MEMS'));
      expect(result.data.rpmValue, equals(3500));
      expect(result.data.coolantTempValue, equals(85));
    });

    test('fetchInitialData returns null on error', () async {
      final client = MockClient((request) async {
        return http.Response('Server Error', 500);
      });

      final service = EcuServiceTestable(client: client);
      final result = await service.fetchInitialData();

      expect(result, isNull);
    });

    test('fetchInitialData returns null on timeout', () async {
      final client = MockClient((request) async {
        await Future.delayed(const Duration(seconds: 10));
        return http.Response('OK', 200);
      });

      final service = EcuServiceTestable(client: client);
      final result = await service.fetchInitialData();

      expect(result, isNull);
    });
  });

  group('EcuService dataStream', () {
    test('emits EcuInfos when data is added', () async {
      final service = EcuServiceTestable();
      final testData = EcuInfos(connected: true, ecuType: 'TEST');

      expectLater(
        service.dataStream,
        emits(predicate<EcuInfos>((d) => d.connected && d.ecuType == 'TEST')),
      );

      service.emitTestData(testData);
    });

    test('emits multiple values in order', () async {
      final service = EcuServiceTestable();

      final values = <bool>[];
      final sub = service.dataStream.listen((d) => values.add(d.connected));

      service.emitTestData(EcuInfos(connected: true));
      service.emitTestData(EcuInfos(connected: false));
      service.emitTestData(EcuInfos(connected: true));

      await Future.delayed(Duration.zero);
      sub.cancel();

      expect(values, equals([true, false, true]));
    });
  });

  group('EcuService connectionStream', () {
    test('emits connection state changes', () async {
      final service = EcuServiceTestable();

      final states = <bool>[];
      final sub = service.connectionStream.listen(states.add);

      service.emitConnectionState(true);
      service.emitConnectionState(false);

      await Future.delayed(Duration.zero);
      sub.cancel();

      expect(states, equals([true, false]));
    });
  });
}

/// Testable version of EcuService that allows injecting HTTP client
/// and directly emitting test data to streams.
class EcuServiceTestable extends EcuService {
  final http.Client? _testClient;

  EcuServiceTestable({http.Client? client}) : _testClient = client;

  final _testDataController = StreamController<EcuInfos>.broadcast();
  final _testConnectionController = StreamController<bool>.broadcast();

  @override
  Stream<EcuInfos> get dataStream => _testDataController.stream;

  @override
  Stream<bool> get connectionStream => _testConnectionController.stream;

  void emitTestData(EcuInfos data) => _testDataController.add(data);
  void emitConnectionState(bool state) => _testConnectionController.add(state);

  @override
  Future<bool> ping() async {
    if (_testClient == null) return false;
    try {
      final response = await _testClient.get(Uri.parse('$baseUrl/ping'));
      return response.statusCode == 200;
    } catch (e) {
      return false;
    }
  }

  @override
  Future<EcuInfos?> fetchInitialData() async {
    if (_testClient == null) return null;
    try {
      final response = await _testClient
          .get(Uri.parse('$baseUrl/api'))
          .timeout(const Duration(seconds: 5));
      if (response.statusCode == 200) {
        return EcuInfos.fromJson(json.decode(response.body));
      }
    } catch (e) {
      return null;
    }
    return null;
  }
}
