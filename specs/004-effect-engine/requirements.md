# Feature 004 â€” Independent Per-Strip Effect Engine

## Goal

Replace the temporary fixed diagnostic scene with a hardware-independent effect
engine. Six fixed strip slots shall render independently while consuming one
coherent, read-only `AudioLevelFrame` and `SpectrumFrame` per LED frame.

## Requirements

- Target Raspberry Pi Pico W, C++17 and Pico SDK; no Arduino, FreeRTOS, heap
  allocation, exceptions, or RTTI in the real-time path.
- Preserve Feature 001 LED transport, Feature 002 capture ownership, Feature
  003 Q15 analysis, USB diagnostics, and audio-first scheduling.
- Support six independent configurations with enabled state, type, compatible
  source, primary/secondary/background RGBW colours, direction, visual gain,
  attack/release milliseconds and bounded effect parameters.
- Support scalar sources Left, Right, Aux, Mono, Bass, Low, Mid and High;
  structured Stereo Left/Right, 32-band Spectrum and Macro Bands sources.
- Support Off, Static Direct RGBW, Static White Boost, Scalar VU, Stereo
  Centre-Out VU, Spectrum Bars, Mirrored Spectrum Zones, Macro Bands, one-band
  frequency, stroboscope, smooth colour cycle, running rainbow, and
  `frequency_comet` effects.
- Keep that generic catalog distinct from the AlexGyver ColorMusic-compatible
  catalog: Gyver VU Gradient, Gyver VU Rainbow, Gyver Frequency 5 Zones, Gyver
  Frequency 3 Zones, Gyver Frequency Full Strip, Gyver Stroboscope, Gyver Ambient
  Static, Gyver Ambient Color Cycle, Gyver Ambient Running Rainbow, Gyver Running
  Frequencies, and Gyver Spectrum Analyzer.
- The engine shall validate a strip proposal before staging it; scene updates
  are all-or-nothing and changes become visible only at a render boundary.
  Generation increases monotonically for each accepted applied change.
- A type change resets only the relevant changed strip state. No strip may
  affect another strip's configuration, temporal state or destination span.
- Effects must not access ADC, DMA, PIO, GPIO, USB, Wi-Fi, flash, LED manager,
  or invoke FFT analysis.
- Spectrum effects use the raw unsmoothed levels published once by Feature 003;
  Feature 003 smoothed fields and diagnostics remain compatible. Per-strip
  effect smoothing is integer, time-based and uses actual elapsed render time.
- Static Direct RGBW preserves the configured four logical channels. Static
  White Boost accepts a `white_drive_percent` of 0..200 and an RGB assist colour
  whose White component must be zero. 0..100 scales only dedicated White;
  100..200 holds White at 255 and scales the assist RGB channels. It is applied
  before the existing strip brightness of 16/255 and future current limiting.
  A 200 percent setting is a logical channel-drive value, not calibrated
  luminous output, and can consume substantially more electrical current.
- Due frames are skipped when the LED manager is busy. Rendering remains 30 Hz
  maximum and is attempted only in a no-ready-audio iteration.
- The compiled reset-default scene is six independent value copies of one
  canonical configuration: Gyver VU Gradient, Stereo Left/Right source, Off
  background, and a level-position Green â†’ Yellow â†’ Orange â†’ Red gradient.
  Channel 1 has no master role or runtime relationship to Channels 2â€“6.
- VU colour modes are Solid, Level-Position Gradient, and Animated Rainbow.
  Gradient position is normalized from each half's logical centre to its end;
  rainbow phase, speed and spacing remain per-strip.
- Dynamic parameters are bounded fixed-size values: Q8 animation speed and
  spacing, fade/decay time, strobe frequency/fade time, and one-band frequency
  selection (three frequencies, Low, Mid, High).
- `color_spacing_q8` is the hue increment per logical pixel, not a request to
  stretch one rainbow cycle over a strip. Direction changes the logical spatial
  order without changing this increment.
- A segmented effect derives every source target, smooths each logical segment
  or zone once per rendered frame, then reuses that cached level for all pixels
  belonging to it. Smoothing is therefore independent of strip length and
  advances by at least one level toward a different target when elapsed time is
  nonzero.
- `reset_default_scene()` is named configuration data containing six canonical
  centre-out VUs. The boot-time physical bring-up instead cycles isolated,
  timed diagnostic scenes through the existing atomic scene-staging API.
- Gyver VU Gradient and Gyver VU Rainbow are stereo centre-out effects. Their
  independent Left and Right halves support per-strip adaptive display gain,
  enabled by default; disabling it preserves the direct input-level response.
- Gyver VU noise floors and hysteresis are raw ADC peak counts in the
  `0..2047` domain. For each side the engine gates raw peak values, subtracts
  the floor safely, normalizes the remaining range to `0..65535`, applies
  attack/release and visual gain, then applies adaptive reference/headroom.
  The gate opens only above its floor and closes at or below
  `max(0, floor - hysteresis)`. A closed gate freezes its reference while the
  smoothed visible value releases normally.
- Gyver Rainbow uses a normalized centre-to-edge span. Its default
  `gyver_rainbow_span_percent=50` covers half a hue cycle per side; phase is
  subtracted from outward position so the pattern travels centre-to-edge.
  Generic rainbow effects retain `color_spacing_q8` as hue increment per pixel.
- `vu_noise_calibrate [duration_ms]` is an optional bounded, non-blocking USB
  calibration command (default 2000 ms; 250..10000 ms). It records raw Left
  and Right maxima, adds a fixed safety margin, and stages the volatile floors
  into currently active Gyver VU configurations. `vu_noise_floors` reports
  current floors; `vu_noise_calibrate cancel` cancels an active measurement.
- Frequency Comet has a source threshold and a configurable 0..100 percent
  tail (default 20 percent). Its nonlinear tail follows the moving head and
  blends from configured background to foreground. Gyver Spectrum subtracts
  `gyver_spectrum_noise_floor` before its peak/reference/normalization path.
- Gyver Frequency 5 Zones, 3 Zones, Full Strip, and Running Frequencies consume
  the shared Mono Low/Mid/High macro levels through a per-strip adaptive event
  detector: fast filtered input, slow average, relative threshold, event
  flash, and decaying event level. They do not use continuous absolute
  brightness as their primary visible trigger.
- Gyver Frequency 5 Zones is `High | Mid | Low | Mid | High`; Gyver Frequency 3
  Zones is `High | Mid | Low`. Direction reverses those logical layouts. Gyver
  Full Strip supports `gyver_priority` (High, Mid, Low) and `strongest_event`.
- Gyver Running Frequencies retains a fixed, per-strip half-history and mirrors
  it from the centre to both ends. Gyver Spectrum Analyzer maps all 32 spectrum
  bands from low at the centre to high at both ends and has adaptive display
  gain enabled by default.
- Gyver Running Frequencies independently selects `gyver_priority` (High,
  Mid, Low) or `strongest_event`; its default is `gyver_priority`. Gyver
  Spectrum additionally requires an adjusted peak of at least
  `gyver_spectrum_minimum_peak` after per-band floor subtraction before it may
  initialize or update its adaptive reference.
- Gyver Stroboscope is always `hard_cut`: foreground is immediately on for its
  configured duty interval and the exact background is immediately restored
  for the remainder. Generic Stroboscope defaults to the same hard-cut mode
  and may explicitly select a fade envelope.
- `scalar_vu` has the stable public identity `linear_vu` and display name
  `Linear VU`. It accepts only Left, Right, Aux or Mono and retains independent
  direction, solid/gradient/rainbow, RGBW palette/background, Q8 gain,
  attack/release, animation speed and rainbow-spacing configuration. It is
  distinct from Stereo Centre-Out VU.
- Global Idle Lighting is a separate, fixed-size staged controller. It blends
  after effects render and before LED transport; all effects continue updating
  beneath it. Its defaults are disabled, startup-idle enabled, 10,000 ms
  silence, 150 ms confirmation, dedicated White `{0,0,0,255}`, Q8 brightness
  256, 750 ms fade to effects, 1500 ms fade to idle, all inputs and all strips.
  Raw Left/Right/Aux peaks use independent floor gates and OR selection.
- `idle_brightness_q8` is exactly `0..256`: 0 is off and 256 is 100 percent
  of `idle_color_rgbw`. Values above 256 are invalid. Non-disruptive Idle
  changes (colour, brightness, thresholds, timings and masks) preserve the
  current transition mix; enabling/disabling resets controller semantics.
- Temporary non-blocking USB bring-up commands are `idle_enable`,
  `idle_disable`, `idle_status`, and `idle_test`. They stage the global Idle
  configuration at the next renderer frame boundary; no persistence is added.
- Gyver reactive effects use configurable RGBW background colour and Q8
  brightness, defaulting to off. Gyver Stroboscope additionally supports
  frequency, duty cycle, fade and that background.

## Definition of done

Completion requires host tests without Pico hardware headers, all existing host
targets passing, a warning-free Pico W firmware build, and telemetry containing
effect generation/frame/timing fields. Physical acceptance remains pending until
the supplied UF2 completes the documented quiet and music tests.
