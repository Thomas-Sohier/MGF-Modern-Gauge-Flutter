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
  final StreamController<EcuInfos> _dataController =
      StreamController<EcuInfos>.broadcast();
  final StreamController<bool> _connectionController =
      StreamController<bool>.broadcast();

  Stream<EcuInfos> get dataStream => _dataController.stream;

  /// Émet `true` quand la connexion WebSocket est établie, `false` à la fermeture.
  Stream<bool> get connectionStream => _connectionController.stream;

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

  Future<EcuInfos?> fetchInitialData() async {
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
        return EcuInfos.fromJson(json.decode(body));
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
  Timer? _reconnectTimer;
  bool _isConnected = false;
  bool _autoReconnect = false;
  int _reconnectAttempts = 0;

  static const _reconnectDelays = [1, 2, 4, 8, 16, 30]; // seconds

  bool get isConnected => _isConnected;

  void connectWebSocket() {
    _autoReconnect = true;
    _reconnectAttempts = 0;
    _connect();
  }

  void _connect() {
    _closeCurrentConnection();

    LogService.info('EcuService: Connecting to WebSocket at $wsUrl...');

    try {
      _channel = WebSocketChannel.connect(Uri.parse(wsUrl));
    } catch (e) {
      LogService.error('EcuService: Failed to create WebSocket channel: $e');
      _scheduleReconnect();
      return;
    }

    _channel!.stream.listen(
      (message) {
        try {
          final data = json.decode(message);
          _dataController.add(EcuInfos.fromJson(data));
          _reconnectAttempts = 0; // reset backoff on successful message
        } catch (e) {
          LogService.error('EcuService: Error parsing WebSocket message: $e');
        }
      },
      onError: (error) {
        LogService.error('EcuService: WebSocket error: $error');
        _isConnected = false;
        _connectionController.add(false);
        _closeCurrentConnection();
        _scheduleReconnect();
      },
      onDone: () {
        LogService.info('EcuService: WebSocket connection closed');
        _isConnected = false;
        _connectionController.add(false);
        _closeCurrentConnection();
        _scheduleReconnect();
      },
      cancelOnError: true,
    );

    // Initial request for data (Go agent expects "." to start session)
    _safeSend('.');
    _isConnected = true;
    _connectionController.add(true);

    // Periodically send "." to keep data flowing
    _periodicTimer = Timer.periodic(const Duration(milliseconds: 100), (_) {
      _safeSend('.');
    });
  }

  void _scheduleReconnect() {
    if (!_autoReconnect) return;
    final delayIndex = _reconnectAttempts.clamp(0, _reconnectDelays.length - 1);
    final delaySecs = _reconnectDelays[delayIndex];
    _reconnectAttempts++;
    LogService.info(
      'EcuService: Reconnecting in ${delaySecs}s (attempt $_reconnectAttempts)...',
    );
    _reconnectTimer?.cancel();
    _reconnectTimer = Timer(Duration(seconds: delaySecs), _connect);
  }

  /// Manually reconnect the WebSocket. Resets backoff. Use this for user-triggered retries.
  void reconnectWebSocket() {
    LogService.info('EcuService: Manual reconnect requested.');
    _reconnectAttempts = 0;
    _autoReconnect = true;
    _connect();
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
    _autoReconnect = false;
    _reconnectTimer?.cancel();
    _reconnectTimer = null;
    _closeCurrentConnection();
    _dataController.close();
    _connectionController.close();
  }
}
