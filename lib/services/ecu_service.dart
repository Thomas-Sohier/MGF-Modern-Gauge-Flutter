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
  StreamSubscription? _channelSubscription;

  final StreamController<EcuInfos> _dataController =
      StreamController<EcuInfos>.broadcast();
  final StreamController<bool> _connectionController =
      StreamController<bool>.broadcast();

  Stream<EcuInfos> get dataStream => _dataController.stream;

  /// Émet `true` à la réception du premier message, `false` à la fermeture.
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
      final response = await http
          .get(Uri.parse('$baseUrl/api'))
          .timeout(const Duration(seconds: 5));
      if (response.statusCode == 200) {
        return EcuInfos.fromJson(json.decode(response.body));
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

  // --- WebSocket ---

  Timer? _reconnectTimer;
  Timer? _stableConnectionTimer;

  bool _isConnected = false;
  bool _autoReconnect = false;
  int _reconnectAttempts = 0;

  static const _reconnectDelays = [1, 2, 4, 8, 16, 30]; // secondes

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

    _channelSubscription = _channel!.stream.listen(
      (message) {
        // Premier message reçu → connexion réellement établie.
        if (!_isConnected) {
          _isConnected = true;
          _connectionController.add(true);
          // Le backoff ne se réinitialise qu'après 5 s de connexion stable.
          _stableConnectionTimer = Timer(const Duration(seconds: 5), () {
            _reconnectAttempts = 0;
          });
        }
        try {
          _dataController.add(EcuInfos.fromJson(json.decode(message)));
        } catch (e) {
          LogService.error('EcuService: Error parsing WebSocket message: $e');
        }
      },
      onError: (error) {
        LogService.error('EcuService: WebSocket error: $error');
        _handleDisconnect();
      },
      onDone: () {
        LogService.info('EcuService: WebSocket connection closed');
        _handleDisconnect();
      },
      cancelOnError: true,
    );
  }

  void _handleDisconnect() {
    // Guard : évite le double-appel si sink.close() re-déclenche onDone.
    if (_channel == null) return;

    final wasConnected = _isConnected;
    _isConnected = false;
    if (wasConnected) {
      _connectionController.add(false);
      _dataController.add(EcuInfos.initial());
    }
    _closeCurrentConnection();
    _scheduleReconnect();
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

  /// Reconnexion manuelle — réinitialise le backoff.
  void reconnectWebSocket() {
    LogService.info('EcuService: Manual reconnect requested.');
    _reconnectAttempts = 0;
    _autoReconnect = true;
    _connect();
  }

  void _closeCurrentConnection() {
    _stableConnectionTimer?.cancel();
    _stableConnectionTimer = null;
    _channelSubscription?.cancel();
    _channelSubscription = null;
    // _channel = null AVANT sink.close() pour que le guard dans
    // _handleDisconnect() court-circuite tout onDone/onError résiduel.
    final channel = _channel;
    _channel = null;
    try {
      channel?.sink.close();
    } catch (_) {}
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
