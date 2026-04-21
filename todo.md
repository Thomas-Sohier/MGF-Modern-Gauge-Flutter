  ---
  1. Fix DigitalDial animation thrashing

  ▎ In lib/ui/widgets/digital_dial.dart, _DigitalDialState.didUpdateWidget resets an AnimationController + rebuilds a Tween + CurvedAnimation every
  ▎ value change. Since EcuProvider notifies at ~10 Hz (100 ms) and the animation is 200 ms, the controller never completes and allocates per tick.
  ▎ Replace the manual controller with a TweenAnimationBuilder<double> (200 ms, Curves.easeOutCubic) that feeds _DialPainter.value, OR keep the
  ▎ controller but reuse one CurvedAnimation/Tween and call animateTo(newValue) instead of reset+forward. Ensure no allocation inside build. Target:
  ▎ RPi 3B+.

  2. Add RepaintBoundary around dynamic painters

  ▎ Wrap the CustomPaint in lib/ui/widgets/digital_dial.dart (build method), lib/ui/widgets/music_dial.dart (line ~33), and
  ▎ lib/ui/widgets/analog_clock.dart (line ~13) in a RepaintBoundary. This isolates repaints from parent layers. Do NOT wrap already-static painters
  ▎ (the one in gauge_background.dart already has one).

  3. Collapse dual DigitalDial in RpmScreen

  ▎ lib/ui/screens/rpm_screen.dart stacks two DigitalDial instances (throttle at line ~105, primary at line ~120), each repainting at 10 Hz. Create a
  ▎ single painter _DualArcDialPainter in a new file lib/ui/widgets/dual_arc_dial.dart that draws both the inner throttle arc (1 segment, radius
  ▎ R*0.7) and outer primary arc (20 segments) in one paint() pass. Wire RpmScreen to use it, with two scalar inputs (throttleValue, primaryValue).
  ▎ Wrap in RepaintBoundary.

  4. Hoist Paint allocations out of paint loops

  ▎ In lib/ui/widgets/digital_dial.dart _DialPainter._drawSegment (lines 197-217), a new Paint is allocated per segment (up to 20×/paint). Promote
  ▎ activePaint, inactivePaint, dangerPaint, dangerInactivePaint to fields initialized in the constructor; mutate only via ..color = … if needed. Same
  ▎  pattern in lib/ui/widgets/analog_clock.dart _AnalogClockPainter.paint (lines 33-43, 93-94): cache dotPaint, shadowPaint, hourTickPaint,
  ▎ handPaint, ridgesPaint, and one shared TextPainter as fields instead of per-paint allocations.

  5. Cache TextPainter layouts in AnalogClock

  ▎ In lib/ui/widgets/analog_clock.dart _AnalogClockPainter.paint (lines 39-79), 4 hour labels (12, 3, 6, 9) call textPainter.layout() each paint.
  ▎ Since the clock paints once per minute but layouts are static per (size, theme), pre-compute the four TextPainters + their offsets inside a
  ▎ memoized _LayoutCache keyed by size.width. Rebuild cache only when size changes. Alternative: render tick+labels once into a ui.Picture via
  ▎ PictureRecorder cached per-size, and canvas.drawPicture(...) in paint().

  6. Remove Material/InkWell ripple inside gauge central display

  ▎ In lib/ui/screens/multi_metric_screen.dart MetricPrimaryDisplay.build (lines 174-201) and MetricIndicator.build (lines 280-287), Material +
  ▎ InkWell cause ripple/hover overlays that trigger GPU saveLayer calls. Replace with GestureDetector(onTap: onCycle, behavior:
  ▎ HitTestBehavior.opaque, child: …). Keep a minimal visual feedback via a stateful AnimatedOpacity if needed — but no InkWell on Pi.

  7. Fix MaterialApp rebuilding on every settings change

  ▎ In lib/app.dart lines 35-47: the outer Selector<SettingsProvider, ThemeMode> is defeated because the builder reads
  ▎ Provider.of<SettingsProvider>(context, listen: true).settings.themeMode. Replace that line with themeMode: status, where status is the Selector's
  ▎ value. This stops MaterialApp rebuilding when unrelated settings (background, enabledScreens) change.

  8. Hoist bottomChildren out of primary-value Selector

  ▎ In lib/ui/screens/multi_metric_screen.dart _MultiMetricScreenState.build (lines 122-147) and lib/ui/screens/rpm_screen.dart _RpmScreenState.build 
  ▎ (lines 119-143): the bottomChildren list + all MetricIndicators are rebuilt inside the Selector for the primary value (i.e. at 10 Hz). Move the
  ▎ bottomChildren list construction OUTSIDE the Selector (compute it once per _primaryIndex change, cache in a late final / StatefulWidget field or
  ▎ useMemoized-equivalent). Only the gauge value + MetricPrimaryDisplay should sit inside the inner Selector.

  9. Reduce Record allocations in MetricIndicator Selector

  ▎ In lib/ui/screens/multi_metric_screen.dart MetricIndicator.build (lines 223-227): Selector<EcuProvider, (double, IconData?)> allocates a Record
  ▎ per evaluation × N indicators × 10 Hz. If metric.icon returns a static IconData (no EcuInfos dependency), split into a single
  ▎ Selector<EcuProvider, double> for value + read the static icon outside. If icon truly is dynamic, keep the record but verify == works (Records
  ▎ compare by value — OK).

  10. Reduce EcuData.fromJson allocation churn

  ▎ In lib/models/ecu_data.dart: EcuData.fromJson allocates 53 num? fields × 10 Hz. The UI only reads ~10 fields (rpm, coolantTemp, batteryVoltage,
  ▎ oilTemp, throttleAngle, mapSensorKpa, throttlePotVoltage, intakeAirTemp, ambientTemp, fuelRailTemp, vehicleSpeed, lambdaMv). Either (a) reduce the
  ▎  class to only those fields, (b) back EcuData with a raw Map<String,dynamic> + typed getters that cast on access, or (c) (preferred) update the Go
  ▎  backend contract to ship only needed fields. Ship option (b) if backend cannot change.

  11. Remove or slow the 100 ms WebSocket heartbeat

  ▎ In lib/services/ecu_service.dart lines 141-145: Timer.periodic(Duration(milliseconds: 100), (_) => _safeSend('.')). Verify with the Go backend
  ▎ whether the periodic '.' kick is needed at 10 Hz or whether one initial '.' suffices (backend pushes spontaneously). If backend pushes
  ▎ autonomously, delete the periodic timer. If it needs periodic kicking, raise interval to 1 s minimum.

  12. Offload color quantization to an isolate

  ▎ In lib/utils/color_util.dart ColorUtil._extractColorsFromImageProvider + QuantizerCelebi().quantize(...) (lines 42-54): this runs on the UI
  ▎ isolate and blocks frames for 100-500 ms on Pi 3B+. Move the quantize call into Isolate.run(() => QuantizerCelebi().quantize(pixels, 128,
  ▎ returnInputPixelToClusterPixel: true)) (or compute). The image→bytes step must still happen on the UI isolate (Flutter restriction) but
  ▎ quantization and Score.score are pure — isolate-safe.

  13. Lower album-art filter quality

  ▎ In lib/ui/screens/music_player_screen.dart _AlbumArt.build line 272: change filterQuality: FilterQuality.high to FilterQuality.medium. High is
  ▎ bicubic and costly on VideoCore IV.

  14. Slow MPRIS position tick from 100 ms to 500 ms

  ▎ In lib/services/mpris_listener.dart line 25: change _positionUpdateInterval = Duration(milliseconds: 100) to Duration(milliseconds: 500). The UI
  ▎ (_CurrentPositionText) only formats whole seconds, so 10 Hz notifications are wasted. Adjust _position += _positionUpdateInterval math (already
  ▎ uses the constant).

  15. Drop ClipOval saveLayer from dashboard shell

  ▎ In lib/ui/screens/dashboard_shell_screen.dart line 71: ClipOval(child: GaugeTexturedBackground(...)) forces a per-frame saveLayer → offscreen
  ▎ buffer. If the physical display is already circular (likely for this project), remove the ClipOval entirely. If not, replace with a Stack where
  ▎ the oval mask is drawn by a single static CustomPainter at the top of the stack (black outside the circle), avoiding saveLayer.

  16. Downscale logo asset

  ▎ Check assets/images/mg_logo.png size (identify or file). If it exceeds the target display resolution (e.g. 800×480 or 480×480), re-export at
  ▎ native resolution to reduce decode time and memory on Pi 3B+. Do not modify Dart code — this is an asset task.

  17. Audit heavy dependencies

  ▎ Inspect pubspec.yaml. The image: ^4.5.4 package is listed but is only pulled in transitively or not used in lib/. Run grep -r "package:image/"
  ▎ lib/ to confirm. If unused, remove from pubspec.yaml. Evaluate whether material_color_utilities alone can replace the palette pipeline in
  ▎ lib/utils/color_util.dart without image.

  18. Make LogService file writes async-batched

  ▎ In lib/services/log_service.dart: Logger + FileOutput writes synchronously on every info() call from the main isolate. Replace FileOutput with a
  ▎ custom LogOutput that buffers lines in an in-memory List<String>, flushing every 2 s or on 64 entries via File.writeAsString(..., mode:
  ▎ FileMode.append) off the main path. Keep ConsoleOutput for debug builds only (kDebugMode).

  19. Tighten analysis_options.yaml

  ▎ In analysis_options.yaml, enable the following lints under linter: rules:
  ▎ prefer_const_constructors: true
  ▎ prefer_const_literals_to_create_immutables: true
  ▎ prefer_const_declarations: true
  ▎ avoid_unnecessary_containers: true
  ▎ use_key_in_widget_constructors: true
  ▎ prefer_final_fields: true
  ▎ unnecessary_lambdas: true
  ▎ Then run dart fix --apply and flutter analyze. Commit the fixes.

  20. Refresh CLAUDE.md to match actual code

  ▎ CLAUDE.md references OdbService, DialData, DialProvider, and MprisProvider (as a concrete class). None exist in lib/. Update the "Architecture"
  ▎ section: data flow is EcuService → Stream<EcuInfos> → EcuProvider (holds EcuInfos) → Selectors in screens. MprisListener is the concrete class
  ▎ (ChangeNotifier) with MprisListenerBase abstract (lib/providers/mpris_provider.dart). Remove mentions of fuel/oilPressure/odometer.

  21. Unify EcuInfos disconnect fallback

  ▎ In lib/services/ecu_service.dart line 156: _dataController.add(EcuInfos()) emits an EcuInfos with ecuData: null, while
  ▎ lib/providers/ecu_provider.dart line 10 initializes with EcuInfos.initial() (has ecuData: EcuData.initial()). Change line 156 to
  ▎ _dataController.add(EcuInfos.initial()) so downstream Selectors don't see null ecuData → fewer ?. chain short-circuits and one less rebuild on
  ▎ disconnect.

  22. Use a single PageController in SettingsScreen

  ▎ In lib/ui/screens/settings_screen.dart _SettingsScreenState._enterCategory (lines 85-94) and _backToRoot (96-104): _pageController.dispose() +
  ▎ PageController() re-allocation on every category change. Keep a single PageController; call _pageController.jumpToPage(0) on enter and
  ▎ .jumpToPage(_rootPage) on back. Dispose only in dispose().

  23. Re-request focus on shell route return

  ▎ In lib/ui/screens/dashboard_shell_screen.dart: after navigating to /settings and back, _focusNode may lose focus → arrow keys dead. Add @override
  ▎ void didChangeDependencies() that calls WidgetsBinding.instance.addPostFrameCallback((_) { if (mounted) _focusNode.requestFocus(); }). Verify
  ▎ autofocus: true still works on first build.

  24. Centralize full route paths

  ▎ In lib/routes/route_names.dart: concatenations like RouteNames.dashboardRoute + RouteNames.rpmRoute appear in >10 files. Add static const String
  ▎ rpmFull = '$dashboardRoute$rpmRoute'; (and equivalents) to RouteNames. Replace all runtime concatenations with these constants. Purely mechanical
  ▎ refactor.

  25. Fix or delete skipped widget tests

  ▎ test/widget_test.dart has tests marked skip: true per CLAUDE.md. Either (a) fix them — regenerate mocks via flutter pub run build_runner build
  ▎ --delete-conflicting-outputs, remove skip, update to current providers — or (b) delete the file. Report which tests pass after removing skip.

  26. Document release-mode build instructions

  ▎ Update README.md: add a "Running on Raspberry Pi 3B+" section with the exact command flutter build linux --release --tree-shake-icons
  ▎ --target-platform=linux-arm. Note that debug builds are 3-5× slower. Add runtime env suggestion: FLUTTER_ENGINE_SWITCH_IMPELLER=false until
  ▎ Impeller-Linux-embedded is stable for VideoCore IV.

  27. Benchmark Impeller vs Skia on Pi 3B+ (investigation)

  ▎ Research task only — do NOT modify code. Run the app on an actual Pi 3B+ (or emulated VC4) in both --release with default renderer (Skia) and with
  ▎  FLUTTER_ENGINE_SWITCH_IMPELLER=true. Capture flutter run --profile timeline for 60 s of idle-RPM gauge animation. Report raster + UI thread
  ▎ median/95p frame times per renderer. Output: one markdown table + recommendation.

  28. Decouple UI refresh rate from data rate

  ▎ Current pipeline: 10 Hz data → 10 Hz notifyListeners → 10 Hz Selector rebuilds. Implement a decoupling layer: in EcuProvider, buffer incoming     
  ▎ EcuInfos in a field but only call notifyListeners() via a Ticker / SchedulerBinding.scheduleFrameCallback at the display refresh rate (capped at 
  ▎ 30 Hz for Pi). This collapses multiple sub-frame data updates into one UI rebuild per frame. File: lib/providers/ecu_provider.dart.               
                  
  29. Subset the JetBrainsMono fonts                                                                                                                  
   
  ▎ assets/fonts/JetBrainsMono-Regular.ttf and -Bold.ttf ship the full glyph set (~200 KB each). The UI uses digits, basic Latin, degree sign, colon, 
  ▎ space, slash. Use pyftsubset (fonttools) to produce subsetted TTFs:
  ▎ pyftsubset JetBrainsMono-Regular.ttf \                                                                                                            
  ▎   --unicodes="U+0020-007E,U+00B0,U+00C0-00FF" \
  ▎   --output-file=JetBrainsMono-Regular.ttf                                                                                                         
  ▎ Same for Bold. Measure size reduction; commit both. Verify glyphs render correctly in all screens.                                                
                                                                                                                                                      
  30. Capture baseline DevTools timeline                                                                                                              
                                                                                                                                                      
  ▎ Investigation task only. Run the app on Pi 3B+ (or closest Linux target) in --profile. Open DevTools Performance tab. Record 30 s of: (a) idle on 
  ▎ RPM screen with live WebSocket data, (b) arrow-key navigation across all screens, (c) MusicPlayerScreen with album art. Export each trace as 
  ▎ .json. Report top-5 widgets by "Rebuild count" and top-5 painters by "Raster time". This establishes a baseline before any change from prompts    
  ▎ 1-15.         

