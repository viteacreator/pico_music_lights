# Feature 003.1 — Ordered Completion Tasks

- [x] Record the 63 ms float-backend failure and 8 ms real capture deadline.
- [x] Add isolated Q15 FFT backend with static Q15 Hann/twiddle tables,
  stage scaling, rounding, saturation, and integer bin power.
- [x] Integrate Q15 backend without changing public spectrum/capture/LED
  transport interfaces.
- [x] Add host Q15-to-float-reference energy tolerance, exact silence,
  1/2/4-count floor suppression, 8-count detection/dominant-bin/leakage,
  signed symmetry, residual DC removal, reference level, strong-input safety,
  contiguous dominant-bin plateau selection, and spectrum
  classification/continuity coverage.
- [x] Remove normal command workflow; add automatic startup, deferred one-time
  CDC startup delivery, renderer enable when available, one-second integer
  `DBG` telemetry, and bounded USB output.
- [x] Add pure renderer-only envelope/spectrum visual normalization and tests.
- [x] Build Pico W Release firmware and inspect map/flash-table placement.
- [x] Run all native host targets in a clean native toolchain after the
  dominant-bin plateau correction.
- [ ] Owner physical validation: Q15 timing, normal continuity counters,
  80/300/1000/6000 Hz, quiet/reference input, music, USB logging, and six
  automatic diagnostic views.

No task changes Feature 002 two-buffer ownership, Feature 001 PIO/DMA
transport, or creates a second FFT per strip.
