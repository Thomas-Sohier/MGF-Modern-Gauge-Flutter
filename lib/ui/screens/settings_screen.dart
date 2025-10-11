import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import 'package:modern_gauge_flutter/models/settings_data.dart';
import 'package:modern_gauge_flutter/providers/settings_provider.dart';
import 'package:modern_gauge_flutter/services/settings_service.dart';

/// The screen for managing application settings.
class SettingsScreen extends StatelessWidget {
  const SettingsScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final settingsProvider = Provider.of<SettingsProvider>(context);
    final currentSettings = settingsProvider.settings;

    return Scaffold(
      backgroundColor: Colors.black,
      appBar: AppBar(
        title: const Text('Settings', style: TextStyle(color: Colors.white)),
        backgroundColor: Colors.transparent,
        elevation: 0,
        iconTheme: const IconThemeData(color: Colors.white),
      ),
      body: SafeArea(
        child: ListView(
          padding: const EdgeInsets.all(16.0),
          children: [
            // --- General Settings ---
            _buildSectionTitle(context, 'General'),
            _buildToggleSetting(
              context,
              title: 'Sound Effects',
              value: currentSettings.soundEnabled,
              onChanged: (bool value) {
                settingsProvider.toggleSound(value);
                SettingsService.saveSettings(settingsProvider.settings);
              },
            ),
            _buildBrightnessSlider(
              context,
              value: currentSettings.screenBrightness,
              onChanged: (double value) {
                settingsProvider.setScreenBrightness(value);
                // Note: Brightness might be saved less frequently or when leaving the screen
                // For this example, we save on change.
                SettingsService.saveSettings(settingsProvider.settings);
              },
            ),

            const SizedBox(height: 20),

            // --- Display Settings ---
            _buildSectionTitle(context, 'Display'),
            _buildThemeModePicker(
              context,
              currentValue: currentSettings.themeMode,
              onChanged: (ThemeModeOption? value) {
                if (value != null) {
                  settingsProvider.setThemeMode(value);
                  SettingsService.saveSettings(settingsProvider.settings);
                }
              },
            ),

            // TODO: Add an option for background image selection here
            // _buildImageSelector(context, currentSettings.backgroundImagePath, (path) {
            //   settingsProvider.setBackgroundImage(path);
            //   SettingsService.saveSettings(settingsProvider.settings);
            // }),
            const SizedBox(height: 20),

            // --- Sleep Mode Settings ---
            _buildSectionTitle(context, 'Sleep Mode'),
            _buildAutoSleepDelayPicker(
              context,
              currentValue: currentSettings.autoSleepDelaySeconds,
              onChanged: (int? value) {
                if (value != null) {
                  settingsProvider.setAutoSleepDelay(value);
                  SettingsService.saveSettings(settingsProvider.settings);
                }
              },
            ),
            _buildWakeUpModePicker(
              context,
              currentValue: currentSettings.wakeUpMode,
              onChanged: (WakeUpMode? value) {
                if (value != null) {
                  settingsProvider.setWakeUpMode(value);
                  SettingsService.saveSettings(settingsProvider.settings);
                }
              },
            ),

            const SizedBox(height: 30),
            _buildResetSettingsButton(context),
          ],
        ),
      ),
    );
  }

  /// Builds a title for a settings section.
  Widget _buildSectionTitle(BuildContext context, String title) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 10.0),
      child: Text(
        title,
        style: Theme.of(context).textTheme.headlineSmall?.copyWith(color: Colors.white70, fontWeight: FontWeight.bold),
      ),
    );
  }

  /// Builds a toggle switch for a boolean setting.
  Widget _buildToggleSetting(
    BuildContext context, {
    required String title,
    required bool value,
    required ValueChanged<bool> onChanged,
  }) {
    return Card(
      color: Colors.grey[850],
      margin: const EdgeInsets.symmetric(vertical: 8.0),
      child: SwitchListTile(
        title: Text(title, style: const TextStyle(color: Colors.white)),
        value: value,
        onChanged: onChanged,
        activeThumbColor: Theme.of(context).primaryColor,
      ),
    );
  }

  /// Builds a slider for screen brightness.
  Widget _buildBrightnessSlider(
    BuildContext context, {
    required double value,
    required ValueChanged<double> onChanged,
  }) {
    return Card(
      color: Colors.grey[850],
      margin: const EdgeInsets.symmetric(vertical: 8.0),
      child: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text('Screen Brightness', style: TextStyle(color: Colors.white)),
            Slider(
              value: value,
              min: 0.1, // Minimum brightness
              max: 1.0, // Maximum brightness
              divisions: 9, // 10 steps from 0.1 to 1.0
              label: '${(value * 100).round()}%',
              onChanged: onChanged,
              activeColor: Theme.of(context).primaryColor,
              inactiveColor: Colors.grey[600],
            ),
          ],
        ),
      ),
    );
  }

  /// Builds a dropdown for ThemeMode selection.
  Widget _buildThemeModePicker(
    BuildContext context, {
    required ThemeModeOption currentValue,
    required ValueChanged<ThemeModeOption?> onChanged,
  }) {
    return Card(
      color: Colors.grey[850],
      margin: const EdgeInsets.symmetric(vertical: 8.0),
      child: ListTile(
        title: const Text('Theme Mode', style: TextStyle(color: Colors.white)),
        trailing: DropdownButton<ThemeModeOption>(
          value: currentValue,
          dropdownColor: Colors.grey[800],
          style: const TextStyle(color: Colors.white),
          underline: Container(), // Remove underline
          onChanged: onChanged,
          items: ThemeModeOption.values.map((ThemeModeOption mode) {
            return DropdownMenuItem<ThemeModeOption>(
              value: mode,
              child: Text(mode.toString().split('.').last.capitalize(), style: const TextStyle(color: Colors.white)),
            );
          }).toList(),
        ),
      ),
    );
  }

  /// Builds a dropdown for Auto Sleep Delay selection.
  Widget _buildAutoSleepDelayPicker(
    BuildContext context, {
    required int currentValue,
    required ValueChanged<int?> onChanged,
  }) {
    final List<Map<String, dynamic>> delayOptions = [
      {'label': 'Never', 'value': 0},
      {'label': '1 Minute', 'value': 60},
      {'label': '5 Minutes', 'value': 300},
      {'label': '10 Minutes', 'value': 600},
      {'label': '30 Minutes', 'value': 1800},
    ];

    return Card(
      color: Colors.grey[850],
      margin: const EdgeInsets.symmetric(vertical: 8.0),
      child: ListTile(
        title: const Text('Auto Sleep Delay', style: TextStyle(color: Colors.white)),
        trailing: DropdownButton<int>(
          value: currentValue,
          dropdownColor: Colors.grey[800],
          style: const TextStyle(color: Colors.white),
          underline: Container(),
          onChanged: onChanged,
          items: delayOptions.map((option) {
            return DropdownMenuItem<int>(
              value: option['value'] as int,
              child: Text(option['label'] as String, style: const TextStyle(color: Colors.white)),
            );
          }).toList(),
        ),
      ),
    );
  }

  /// Builds a dropdown for Wake Up Mode selection.
  Widget _buildWakeUpModePicker(
    BuildContext context, {
    required WakeUpMode currentValue,
    required ValueChanged<WakeUpMode?> onChanged,
  }) {
    return Card(
      color: Colors.grey[850],
      margin: const EdgeInsets.symmetric(vertical: 8.0),
      child: ListTile(
        title: const Text('Wake Up Mode', style: TextStyle(color: Colors.white)),
        trailing: DropdownButton<WakeUpMode>(
          value: currentValue,
          dropdownColor: Colors.grey[800],
          style: const TextStyle(color: Colors.white),
          underline: Container(),
          onChanged: onChanged,
          items: WakeUpMode.values.map((WakeUpMode mode) {
            return DropdownMenuItem<WakeUpMode>(
              value: mode,
              child: Text(mode.toString().split('.').last.capitalize(), style: const TextStyle(color: Colors.white)),
            );
          }).toList(),
        ),
      ),
    );
  }

  /// Builds a button to reset all settings to their default values.
  Widget _buildResetSettingsButton(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 20.0),
      child: ElevatedButton(
        style: ElevatedButton.styleFrom(
          backgroundColor: Colors.red.withOpacity(0.8), // Button background color
          foregroundColor: Colors.white, // Text color
          padding: const EdgeInsets.symmetric(vertical: 15),
          shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
        ),
        onPressed: () {
          // Show a confirmation dialog before resetting
          showDialog(
            context: context,
            builder: (BuildContext dialogContext) {
              return AlertDialog(
                backgroundColor: Colors.grey[900], // Dark background for dialog
                title: const Text('Reset Settings?', style: TextStyle(color: Colors.white)),
                content: const Text(
                  'Are you sure you want to reset all settings to default values?',
                  style: TextStyle(color: Colors.white70),
                ),
                actions: <Widget>[
                  TextButton(
                    child: const Text('Cancel', style: TextStyle(color: Colors.white)),
                    onPressed: () {
                      Navigator.of(dialogContext).pop(); // Dismiss dialog
                    },
                  ),
                  TextButton(
                    child: const Text('Reset', style: TextStyle(color: Colors.redAccent)),
                    onPressed: () {
                      Provider.of<SettingsProvider>(context, listen: false).resetSettings();
                      SettingsService.clearSettings(); // Clear persistent storage
                      Navigator.of(dialogContext).pop(); // Dismiss dialog
                      ScaffoldMessenger.of(
                        context,
                      ).showSnackBar(const SnackBar(content: Text('Settings reset to default!')));
                    },
                  ),
                ],
              );
            },
          );
        },
        child: const Text('Reset All Settings', style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
      ),
    );
  }
}

// Extension to capitalize the first letter of a string
extension StringExtension on String {
  String capitalize() {
    if (isEmpty) return this;
    return "${this[0].toUpperCase()}${substring(1)}";
  }
}
