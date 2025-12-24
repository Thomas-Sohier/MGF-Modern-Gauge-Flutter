import 'dart:io';
import 'package:intl/intl.dart';
import 'package:logger/logger.dart';
import 'package:path_provider/path_provider.dart';

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
    final directory = await getApplicationDocumentsDirectory();
    _logDirectory = '${directory.path}/logs';
    await Directory(_logDirectory).create(recursive: true);

    final logFile = File('$_logDirectory/app_${DateFormat('yyyy-MM-dd').format(DateTime.now())}.log');
    _level = Level.info;
    _logger = Logger(
      output: MultiOutput([ConsoleOutput(), FileOutput(file: logFile)]),
      printer: SimplePrinter(printTime: true, colors: false),
    );
    _deleteOldLogs();
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
