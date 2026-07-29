# Feature 003 — Ordered Tasks

1. Add public spectrum types and centralized constants; compile them in firmware and host tests.
2. Add the `CenteredMonoBlock` handoff from Feature 002; test ownership, sequence, and opposite-phase cancellation.
3. Add static window accumulation, 50% overlap, and Hann window generation and application; host-test accumulation and overlap.
4. Add and test the isolated FFT backend with bin-to-frequency and individual tones.
5. Add Hann/FFT-normalized positive-bin power; test amplitude repeatability and DC exclusion.
6. Add direct Bass/Low/Mid/High aggregation from normalized bins; test exact macro ranges.
7. Add the corrected compile-time 32-band mapping and normalized aggregation; test every bin is owned exactly once.
8. Add floor, gain, compression, clamp, and attack/release states; test response.
9. Add pure resampler for 5/8/16/32 outputs; test weighted coverage.
10. Feed centered mono blocks from application processing; measure and report timing/drop count.
11. Add rate-limited USB diagnostics.
12. Add separate 60 Hz diagnostic LED renderer using public APIs.
13. Build, run host tests, and conduct physical tone/music validation.

Every task adds new sources to CMake immediately, preserves a Pico W build,
runs relevant host tests, and identifies remaining physical verification.
