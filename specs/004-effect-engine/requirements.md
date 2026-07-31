# Feature 004 — Independent Per-Strip Effect Engine

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
- Support Off, Static Direct RGBW, Static White Boost, Scalar VU, Stereo Centre-Out VU, Spectrum Bars,
  Mirrored Spectrum Zones and Macro-Band effects.
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
- Default compiled scene: 16-segment spectrum; five-zone mirrored spectrum;
  four macro bands; stereo centre-out VU; Mono scalar VU; Aux scalar VU.

## Definition of done

Completion requires host tests without Pico hardware headers, all existing host
targets passing, a warning-free Pico W firmware build, and telemetry containing
effect generation/frame/timing fields. Physical acceptance remains pending until
the supplied UF2 completes the documented quiet and music tests.
