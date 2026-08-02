# Feature 004 — Tasks

- [x] Define fixed engine configuration, state, source compatibility and
  atomic pending-scene API.
- [x] Publish unsmoothed converted spectrum levels while preserving Feature 003
  smoothed diagnostic fields.
- [x] Implement bounded hardware-independent Off, Static, scalar, stereo,
  spectrum, mirrored-zone and macro renderers.
- [x] Add integer elapsed-time per-strip attack/release state.
- [x] Add ColorMusic-relevant VU, frequency, strobe, ambient and spectrum
  behaviours with bounded independent state.
- [x] Set six value-copied canonical Stereo Centre-Out gradient VU defaults.
- [x] Add Static Direct RGBW and bounded White Boost modes with integer output
  conversion and configuration validation.
- [x] Expose controlled read/stage/scene/default/generation runtime APIs.
- [x] Refine per-strip reset compatibility for source, geometry, mode and
  enable transitions while retaining harmless visual changes.
- [x] Preserve the named reset-default scene and add isolated atomic temporary
  diagnostic scenes for physical bring-up.
- [x] Preserve 30 Hz, LED-busy skip, and audio-priority scheduling.
- [x] Stabilize per-pixel hue spacing, per-zone once-per-frame smoothing,
  minimum-step convergence, edge geometry, and independent effect state.
- [ ] Configure, compile and execute all five warning-enabled native host
  targets after the stabilization regressions: `led_logic_tests`,
  `audio_processing_tests`, `spectrum_analysis_tests`,
  `diagnostic_renderer_tests`, and `effect_engine_tests`.
- [x] Build Pico W Release firmware, inspect static memory, and update
  diagnostics.
- [ ] Complete quiet-input and ordinary-music physical validation.
