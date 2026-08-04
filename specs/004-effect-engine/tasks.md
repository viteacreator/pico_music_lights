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
- [x] Rename the generic circular running-frequency effect to
  `frequency_comet` and add separate Gyver-compatible effect identifiers.
- [x] Add bounded per-strip adaptive Low/Mid/High event state, auto-gain
  references, RGBW half-history, parameter validation, and reset rules.
- [x] Implement Gyver stereo VU, adaptive frequency layouts, Full Strip policy,
  mirrored running frequencies, mirrored 32-band spectrum, and RGBW
  ambient/strobe variants without changing generic effects.
- [x] Update reset-default data to six Gyver VU Gradient configurations and
  expand isolated diagnostic scenes to cover Gyver and generic catalogs.
- [x] Add focused Gyver catalog host regressions for event layouts, policies,
  auto gain, centre history, short spans, reset data and scene cycling.
- [ ] Configure, compile and execute all five warning-enabled native host
  targets after the stabilization regressions: `led_logic_tests`,
  `audio_processing_tests`, `spectrum_analysis_tests`,
  `diagnostic_renderer_tests`, and `effect_engine_tests`.
- [x] Build Pico W Release firmware, inspect static memory, and update
  diagnostics.
- [x] Correct raw-domain Gyver VU gating and release, normalized rainbow span,
  reactive background interpolation, spectrum noise rejection, strobe phases,
  comet tail orientation, and generic Macro Bands mapping.
- [x] Add bounded volatile USB VU-noise calibration and rate-limited VU
  telemetry through the public staged-configuration boundary.
- [ ] Complete quiet-input and ordinary-music physical validation.
