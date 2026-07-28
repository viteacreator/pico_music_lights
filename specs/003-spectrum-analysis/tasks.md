# Feature 003 — Ordered Tasks

1. Add public types and centralized constants; compile them in firmware and host tests.
2. Add static window accumulation/overlap and precomputed Hann support; host-test ownership and overlap.
3. Add isolated in-project FFT backend; host-test bin-to-frequency and tones.
4. Add positive-bin power and direct Bass/Low/Mid/High aggregation; test ranges.
5. Add the exact validated 32-band table and normalized aggregation; test no gaps/overlaps.
6. Add floor, gain, compression, clamp, and attack/release states; test response.
7. Add pure resampler for 5/8/16/32 outputs; test weighted coverage.
8. Feed centered mono blocks from application processing; measure and report timing/drop count.
9. Add rate-limited USB diagnostics.
10. Add separate 60 Hz diagnostic LED renderer using public APIs.
11. Build, run host tests, and conduct physical tone/music validation.

Every task adds new sources to CMake immediately, preserves a Pico W build,
runs relevant host tests, and identifies remaining physical verification.
