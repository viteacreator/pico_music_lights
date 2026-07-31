# Feature 004 — Tasks

- [x] Define fixed engine configuration, state, source compatibility and
  atomic pending-scene API.
- [x] Publish unsmoothed converted spectrum levels while preserving Feature 003
  smoothed diagnostic fields.
- [x] Implement bounded hardware-independent Off, Static, scalar, stereo,
  spectrum, mirrored-zone and macro renderers.
- [x] Add integer elapsed-time per-strip attack/release state.
- [x] Add Static Direct RGBW and bounded White Boost modes with integer output
  conversion and configuration validation.
- [x] Expose controlled read/stage/scene/default/generation runtime APIs.
- [x] Refine per-strip reset compatibility for source, geometry, mode and
  enable transitions while retaining harmless visual changes.
- [x] Replace fixed diagnostic-scene ownership with the engine default scene.
- [x] Preserve 30 Hz, LED-busy skip, and audio-priority scheduling.
- [ ] Add and run independent warning-enabled `effect_engine_tests` (source
  added and warning-checked by cross compilation; native execution remains
  pending a usable Windows host compiler).
- [x] Build Pico W Release firmware, inspect static memory, and update
  diagnostics.
- [ ] Complete quiet-input and ordinary-music physical validation.
