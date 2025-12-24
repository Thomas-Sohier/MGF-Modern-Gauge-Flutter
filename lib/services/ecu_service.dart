import 'dart:async';
import 'dart:convert';
import 'package:http/http.dart' as http;
import 'package:web_socket_channel/web_socket_channel.dart';
import 'package:modern_gauge_flutter/models/ecu_data.dart';
import 'package:modern_gauge_flutter/services/log_service.dart';

class EcuService {
  final String baseUrl;
  final String wsUrl;

  WebSocketChannel? _channel;
  final StreamController<EcuData> _dataController = StreamController<EcuData>.broadcast();

  Stream<EcuData> get dataStream => _dataController.stream;

  EcuService({this.baseUrl = 'http://localhost:8080', this.wsUrl = 'ws://localhost:8080/ws'});

  // --- HTTP Methods ---

  Future<bool> ping() async {
    try {
      final response = await http.get(Uri.parse('$baseUrl/ping'));
      return response.statusCode == 200;
    } catch (e) {
      LogService.error('EcuService: Ping failed: $e');
      return false;
    }
  }

  Future<EcuData?> fetchInitialData() async {
    try {
      final response = await http.get(Uri.parse('$baseUrl/api'));
      if (response.statusCode == 200) {
        return EcuData.fromJson(json.decode(response.body));
      }
    } catch (e) {
      LogService.error('EcuService: Fetch initial data failed: $e');
    }
    return null;
  }

  Future<void> setEcuType(String name) async {
    try {
      await http.get(Uri.parse('$baseUrl/ecu/$name'));
    } catch (e) {
      LogService.error('EcuService: Set ECU type failed: $e');
    }
  }

  Future<void> setSerialPort(String name) async {
    try {
      await http.get(Uri.parse('$baseUrl/serialPort/$name'));
    } catch (e) {
      LogService.error('EcuService: Set serial port failed: $e');
    }
  }

  Future<void> sendCommand(String command) async {
    try {
      await http.get(Uri.parse('$baseUrl/command/$command'));
    } catch (e) {
      LogService.error('EcuService: Send command failed: $e');
    }
  }

  // --- WebSocket Methods ---

  void connectWebSocket() {
    _channel = WebSocketChannel.connect(Uri.parse(wsUrl));

    LogService.info('EcuService: WebSocket connected to $wsUrl');

    _channel!.stream.listen(
      (message) {
        try {
          final data = json.decode(message as String);
          _dataController.add(EcuData.fromJson(data));
        } catch (e) {
          LogService.error('EcuService: Error parsing WebSocket message: $e');
        }
      },
      onError: (error) {
        LogService.error('EcuService: WebSocket error: $error');
        _reconnect();
      },
      onDone: () {
        LogService.info('EcuService: WebSocket connection closed');
        _reconnect();
      },
    );

    // Initial request for data via WebSocket (Go agent expects "." to start session)
    _channel!.sink.add('.');

    // Periodically send "." to keep data flowing if needed by the agent logic
    Timer.periodic(const Duration(milliseconds: 100), (timer) {
      if (_channel != null) {
        _channel!.sink.add('.');
      } else {
        timer.cancel();
      }
    });
  }

  void _reconnect() {
    _channel = null;
    Future.delayed(const Duration(seconds: 2), () {
      LogService.info('EcuService: Reconnecting...');
      connectWebSocket();
    });
  }

  void dispose() {
    _channel?.sink.close();
    _dataController.close();
  }
}
