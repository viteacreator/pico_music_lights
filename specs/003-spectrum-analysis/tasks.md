# Feature 003 — Completed Implementation Tasks

- [x] Define shared spectrum frame, 32-band mapping, macro ranges, and
  centered-mono handoff.
- [x] Implement 1,024-sample 50%-overlap Hann/FFT analysis with normalized
  accumulated energy, smoothing, and sequence continuity accounting.
- [x] Add pure weighted 5/8/16/32 spectrum resampling and native spectrum
  tests.
- [x] Add raw pre-floor FFT diagnostics from the existing FFT iteration.
- [x] Move runtime LED ownership out of obsolete blocking bring-up code into a
  diagnostic-renderer module; preserve six tested Feature 001 configurations.
- [x] Add pure strip-span diagnostic primitives and a separate host renderer
  test target.
- [x] Add the temporary default-off six-strip scene and 60 Hz asynchronous LED
  scheduling.
- [x] Add nonblocking USB commands, status counters, and a three-second quiet
  input measurement mode.
- [x] Build the Pico W firmware, inspect all host-test target dependencies, and
  syntax-check each host target with the available C++ compiler. Native CTest
  execution for the new renderer target remains pending a native toolchain.
- [ ] Perform owner-led physical validation: quiet noise capture, timing,
  80/300/1000/6000 Hz tones, reference tone where practical, music, six-strip
  mapping, and continuity counters.

Every source is introduced in its corresponding CMake target. Renderer tests
remain hardware-independent; no renderer, FFT, or LED transport work runs in
an interrupt.
