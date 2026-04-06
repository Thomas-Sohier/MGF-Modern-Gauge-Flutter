import 'dart:async';
import 'dart:convert';
import 'dart:isolate';
import 'package:http/http.dart' as http;
import 'package:web_socket_channel/web_socket_channel.dart';
import 'package:modern_gauge_flutter/models/ecu_data.dart';
import 'package:modern_gauge_flutter/services/log_service.dart';

class EcuService {
  final String baseUrl;
  final String wsUrl;

  WebSocketChannel? _channel;
  final StreamController<EcuData> _dataController =
      StreamController<EcuData>.broadcast();

  Stream<EcuData> get dataStream => _dataController.stream;

  EcuService({
    this.baseUrl = 'http://localhost:8080',
    this.wsUrl = 'ws://localhost:8080/ws',
  });

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
      final url = '$baseUrl/api';
      // Run the HTTP request in a separate isolate to avoid blocking the main thread.
      // Only primitive types (String) can be passed between isolates.
      final body = await Isolate.run(() async {
        final response = await http
            .get(Uri.parse(url))
            .timeout(const Duration(seconds: 5));
        return response.statusCode == 200 ? response.body : null;
      });
      if (body != null) {
        return EcuData.fromJson(json.decode(body));
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

  Timer? _periodicTimer;
  bool _isConnected = false;

  bool get isConnected => _isConnected;

  void connectWebSocket() {
    _closeCurrentConnection();

    LogService.info('EcuService: Connecting to WebSocket at $wsUrl...');

    try {
      _channel = WebSocketChannel.connect(Uri.parse(wsUrl));
    } catch (e) {
      LogService.error('EcuService: Failed to create WebSocket channel: $e');
      return;
    }

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
        _isConnected = false;
        _closeCurrentConnection();
      },
      onDone: () {
        LogService.info('EcuService: WebSocket connection closed');
        _isConnected = false;
        _closeCurrentConnection();
      },
      cancelOnError: true,
    );

    // Initial request for data (Go agent expects "." to start session)
    _safeSend('.');
    _isConnected = true;

    // Periodically send "." to keep data flowing
    _periodicTimer = Timer.periodic(const Duration(milliseconds: 100), (_) {
      _safeSend('.');
    });
  }

  /// Manually reconnect the WebSocket. Use this for user-triggered retries.
  void reconnectWebSocket() {
    LogService.info('EcuService: Manual reconnect requested.');
    connectWebSocket();
  }

  void _safeSend(String message) {
    try {
      _channel?.sink.add(message);
    } catch (e) {
      LogService.error('EcuService: Failed to send message: $e');
    }
  }

  void _closeCurrentConnection() {
    _periodicTimer?.cancel();
    _periodicTimer = null;
    try {
      _channel?.sink.close();
    } catch (_) {}
    _channel = null;
  }

  void dispose() {
    _closeCurrentConnection();
    _dataController.close();
  }
}
