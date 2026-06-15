import 'package:flutter_test/flutter_test.dart';
import 'package:modern_gauge_flutter/models/navigation_info.dart';

void main() {
  const iconBase = 'http://localhost:8080/api/navigation/icon';

  group('NavigationInfo.fromSnapshot', () {
    test('parses an active snapshot with icon', () {
      final nav = NavigationInfo.fromSnapshot({
        'navigation': {
          'active': true,
          'instruction': 'Turn right onto Rue de Rivoli',
          'distance': '200 m',
          'eta': '14:32',
          'maneuver_icon_id': 'abc123',
        },
        'icon_id': 'abc123',
        'has_icon': true,
      }, iconBaseUrl: iconBase);

      expect(nav.active, isTrue);
      expect(nav.instruction, 'Turn right onto Rue de Rivoli');
      expect(nav.distance, '200 m');
      expect(nav.eta, '14:32');
      expect(nav.iconUrl, '$iconBase?iconId=abc123');
    });

    test('inactive snapshot clears fields and icon', () {
      final nav = NavigationInfo.fromSnapshot({
        'navigation': {
          'active': false,
          'instruction': null,
          'distance': null,
          'eta': null,
          'maneuver_icon_id': null,
        },
        'icon_id': '',
        'has_icon': false,
      }, iconBaseUrl: iconBase);

      expect(nav.active, isFalse);
      expect(nav.instruction, isNull);
      expect(nav.distance, isNull);
      expect(nav.eta, isNull);
      expect(nav.iconUrl, isNull);
    });

    test('no iconUrl when has_icon is false even if id present', () {
      final nav = NavigationInfo.fromSnapshot({
        'navigation': {'active': true, 'instruction': 'Go'},
        'icon_id': 'abc123',
        'has_icon': false,
      }, iconBaseUrl: iconBase);

      expect(nav.iconUrl, isNull);
    });

    test('empty strings are treated as null', () {
      final nav = NavigationInfo.fromSnapshot({
        'navigation': {
          'active': true,
          'instruction': '',
          'distance': '',
          'eta': '',
        },
        'has_icon': false,
      }, iconBaseUrl: iconBase);

      expect(nav.instruction, isNull);
      expect(nav.distance, isNull);
      expect(nav.eta, isNull);
    });

    test('icon id is URL-encoded', () {
      final nav = NavigationInfo.fromSnapshot({
        'navigation': {'active': true},
        'icon_id': 'a b/c',
        'has_icon': true,
      }, iconBaseUrl: iconBase);

      expect(nav.iconUrl, '$iconBase?iconId=a%20b%2Fc');
    });

    test('missing fields default safely', () {
      final nav = NavigationInfo.fromSnapshot({}, iconBaseUrl: iconBase);

      expect(nav.active, isFalse);
      expect(nav.instruction, isNull);
      expect(nav.iconUrl, isNull);
    });
  });
}
