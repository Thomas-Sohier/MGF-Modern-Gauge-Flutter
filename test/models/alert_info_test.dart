import 'package:flutter_test/flutter_test.dart';
import 'package:modern_gauge_flutter/models/alert_info.dart';

void main() {
  group('AlertInfo.fromJson', () {
    test('parses a full alert', () {
      final alert = AlertInfo.fromJson({
        'app': 'Signal',
        'title': 'Alice',
        'text': 'Hi there',
        'posted_at': 1700000000000,
      });

      expect(alert.app, 'Signal');
      expect(alert.title, 'Alice');
      expect(alert.text, 'Hi there');
      expect(
        alert.postedAt,
        DateTime.fromMillisecondsSinceEpoch(1700000000000),
      );
    });

    test('missing fields default to empty / epoch zero', () {
      final alert = AlertInfo.fromJson({});

      expect(alert.app, '');
      expect(alert.title, '');
      expect(alert.text, '');
      expect(alert.postedAt, DateTime.fromMillisecondsSinceEpoch(0));
    });
  });
}
