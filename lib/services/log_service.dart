import 'dart:async';
import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:intl/intl.dart';
import 'package:logger/logger.dart';
import 'package:path_provider/path_provider.dart';

class _BufferedFileOutput extends LogOutput {
  final File file;
  final List<String> _buffer = [];
  Timer? _flushTimer;
  static const _maxBufferSize = 64;
  static const _flushInterval = Duration(seconds: 2);

  _BufferedFileOutput({required this.file});

  Future<void> _flush() async {
    _flushTimer?.cancel();
    _flushTimer = null;

    if (_buffer.isEmpty) return;

    final lines = _buffer.join('\n');
    _buffer.clear();

    try {
      await file.writeAsString('$lines\n', mode: FileMode.append, flush: true);
    } catch (e) {
      // Visible dans journalctl -u flutter-pi
      stderr.writeln('[LogService] écriture impossible dans ${file.path}: $e');
    }
  }

  @override
  void output(OutputEvent event) {
    _buffer.addAll(event.lines);

    if (_buffer.length >= _maxBufferSize) {
      unawaited(_flush());
    } else {
      _scheduleFlush();
    }
  }

  void _scheduleFlush() {
    _flushTimer ??= Timer(_flushInterval, () {
      unawaited(_flush());
    });
  }

  @override
  Future<void> destroy() async {
    await _flush();
    await super.destroy();
  }
}

class LogService {
  late final Logger _logger;
  late final Level _level;
  late final String _logDirectory;
  static LogService? _instance;

  LogService._internal();

  static LogService get instance {
    return _instance!;
  }

  static Future<void> initialize() async {
    if (_instance == null) {
      _instance = LogService._internal();
      await _instance!._init();
    }
  }

  Future<void> _init() async {
    // Surchargeable via systemd : Environment=MGF_LOG_DIR=/chemin
    _logDirectory = Platform.environment['MGF_LOG_DIR'] ??
        '/data/.local/share/flutter-pi/logs';
    _level = Level.info;

    final outputs = <LogOutput>[];
    if (kDebugMode) {
      outputs.add(ConsoleOutput());
    }
    try {
      await Directory(_logDirectory).create(recursive: true);
      final logFile = File('$_logDirectory/app_${DateFormat('yyyy-MM-dd').format(DateTime.now())}.log');
      outputs.add(_BufferedFileOutput(file: logFile));
      stdout.writeln('[LogService] logs fichier dans $_logDirectory');
    } catch (e) {
      stderr.writeln(
          '[LogService] impossible de créer $_logDirectory: $e — logs fichier désactivés');
    }

    _logger = Logger(
      // Le filtre par défaut (DevelopmentFilter) bloque tout en release.
      filter: ProductionFilter(),
      output: MultiOutput(outputs),
      printer: SimplePrinter(printTime: true, colors: false),
      level: _level,
    );
    unawaited(Future.delayed(const Duration(seconds: 5), _deleteOldLogs));
    info("[LogService] - init.");
    info("[LogService] - log at $_logDirectory.");
  }

  void _log(Level level, dynamic message, [dynamic error, StackTrace? stackTrace]) {
    if (_level.index > level.index) return;
    _logger.log(level, message, error: error, stackTrace: stackTrace);
  }

  static void log(Level level, dynamic message, [dynamic error, StackTrace? stackTrace]) {
    LogService.instance._log(level, message, error, stackTrace);
  }

  static void debug(dynamic message, [dynamic error, StackTrace? stackTrace]) {
    log(Level.debug, message, error, stackTrace);
  }

  static void info(dynamic message, [dynamic error, StackTrace? stackTrace]) {
    log(Level.info, message, error, stackTrace);
  }

  static void warning(dynamic message, [dynamic error, StackTrace? stackTrace]) {
    log(Level.warning, message, error, stackTrace);
  }

  static void error(dynamic message, [dynamic error, StackTrace? stackTrace]) {
    log(Level.error, message, error, stackTrace);
  }

  Future<void> _deleteOldLogs() async {
    final thirtyDaysAgo = DateTime.now().subtract(const Duration(days: 30));
    final logDir = Directory(_logDirectory);

    if (await logDir.exists()) {
      final files = logDir.listSync();
      for (var file in files) {
        if (file is File) {
          final fileStat = await file.stat();
          if (fileStat.modified.isBefore(thirtyDaysAgo)) {
            await file.delete();
          }
        }
      }
    }
  }
}
