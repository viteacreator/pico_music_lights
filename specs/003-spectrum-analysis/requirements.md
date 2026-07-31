# Feature 003.1 — Runtime Stabilization and Q15 Spectrum Backend

## Purpose

Feature 003.1 corrects the measured real-time failure of the original
floating-point spectrum implementation while preserving Feature 001 LED
transport and Feature 002 capture ownership. The prior Pico W measurement was
about 63 ms per completed spectrum analysis. Its observed consequences were
continuous ADC block drops, missing audio blocks, and invalidated spectrum
windows.

Processing should preferably remain below the approximately 8 ms completed
audio-block period. The practical hard requirement is comfortably below the
approximately 16 ms spectrum-update cadence, with no growth in continuity
counters as the acceptance condition. Physical testing has observed
`fft_max_us=12295` and `audio_work_max_us=12728` with zero continuity losses
and zero LED timeouts; the extended final validation remains pending.

## Preserved public contract

The following remain unchanged:

- `CenteredMonoBlock` input generated once by Feature 002;
- `SpectrumAnalyzer::push()` and `SpectrumFrame`;
- 1,024 samples at 32 kHz Mono, 50% overlap, and 16 ms intended output cadence;
- residual mean removal, bins 1–384, the 32-band table, and Bass/Low/Mid/High
  ranges;
- sequence discontinuity, missing-block, and dropped-window semantics;
- public spectrum level range 0–65535;
- one shared spectrum result for all six temporary diagnostic strips.

No FFT, ADC processing, or LED transport work may run in an interrupt. Aux is
not part of spectrum analysis.

## Q15 backend

The Pico production backend is a 1,024-point radix-2 fixed-point transform. It
uses a flash-resident 1,024-entry Q15 Hann table and 512 Q15 complex twiddles;
there is no runtime trigonometry, dynamic allocation, or float FFT work.

| Quantity | Representation and rule |
|---|---|
| Input | signed Q0 ADC-centred counts, normally approximately -2048…2047 |
| Hann | Q15, 0…32767 representing 0…approximately 1 |
| Twiddle | complex Q15 cosine and negative sine |
| Work bins | signed integer ADC-count domain after Q15 coefficient multiply |
| Stage scaling | every radix-2 stage divides both butterfly results by two, using signed round-to-nearest with ties-to-even |
| Total FFT scale | `1 / 1024`; bins equal conventional windowed DFT bins divided by 1,024 |
| Saturation | every Q15 complex multiply and butterfly output is clamped to signed 16-bit range |
| Power | `2 × (real² + imaginary²)` accumulated in `uint64_t` |

Let `H = 0.3746337890625`, the Hann mean-square normalization retained by the
previous public contract. The Q15 raw bin energy is in squared input-count
units after the intentional FFT `1 / 1024` scale. The analyzer converts it as:

```text
normalized_energy = q15_energy / H
clean_energy = max(0, normalized_energy - noise_floor_per_bin × bin_count)
level = clamp(65535 × sqrt(clean_energy × gain / reference_energy), 0, 65535)
```

Thus `noise_floor_per_bin = 1.0`, `gain = 1.0`, and
`reference_energy = 32768` retain their prior normalized-energy meaning.
The final 32 display and 4 macro conversions use an integer Q32 square-root
evaluation of the same approved level formula. The two telemetry-only aggregate
diagnostic values are converted to float after integer accumulation. Transform,
bin power, aggregation, and level conversion are integer-only.

Ties-to-even is required for stage division: `+1 / 2` and `-1 / 2` both round
to zero, while exact half-way values whose retained integer bit is odd round to
the adjacent even value. This signed-symmetric rule prevents one-count
butterfly residues from surviving all ten scaled stages as false broadband
energy. It does not change the `1 / 1024` total FFT scale or the calibration
constants.

Dominant-bin reporting is integer-only. A unique nonzero maximum reports its
own bin. A contiguous equal-energy maximum plateau reports its centre; for an
even-width plateau the lower central bin is selected. Separate equal maxima
retain the first, lowest-frequency plateau. Exact silence reports bin 0.

## Automatic runtime and debug output

After boot the firmware initializes USB debug output, then the optional LED
runtime, then mandatory audio capture, spectrum processing, and the temporary
renderer. When at least one strip is usable, the renderer is automatically
enabled before continuous capture begins. If LED initialization fails, audio
still starts in analysis-only mode; if audio initialization fails, the firmware
remains fatal.

There is no normal serial command workflow. Startup information is retained in
a fixed static buffer. Audio and renderer operation begin immediately; when
USB CDC first becomes connected and has room for the complete startup block,
the block is emitted once. A late-connected terminal therefore still receives
the build identifier, initialization results, usable strips, sample rates,
FFT size/overlap/backend, DSP constants, installed pixels, and renderer state.
Telemetry begins only after that one-time delivery.

USB stdout backpressure is bounded to 1 ms for fatal output. Normal telemetry
is formatted into one fixed 512-byte buffer and written directly to a 512-byte
CDC TX buffer only when the complete report fits; otherwise that report is
skipped. Its raw-power fields are integer normalized-power milli-units
(`raw_mean_milli`, `raw_max_milli`), so its once-per-second `snprintf` path has
no floating-point format conversion. It must not hold the main loop for a
significant fraction of an audio block.

Telemetry additionally reports `audio_work_us` and `audio_work_max_us`. These
measure the complete blocking normal-loop audio path for a ready capture block:
Feature 002 block processing plus `SpectrumAnalyzer::push()`, excluding the
separate asynchronous LED renderer call. LED telemetry is cumulative from boot:
`led_frames_started`, `led_frames_completed`, `led_frame_timeouts`, and
`led_last_status`. A status of 13 is `transmission_timeout`.

`fft_us` is the duration of the latest completed spectrum analysis.
`audio_work_us` is the duration of the latest processed audio block and can be
small for a block that does not produce an FFT. `audio_work_max_us` is the
maximum complete audio-block processing duration. `led_frames_started` and
`led_frames_completed` can differ temporarily by one while an asynchronous
frame is active. `led_frames_skipped_busy` counts only a due 30 Hz diagnostic
render slot that could not start because the LED manager was busy; it excludes
ordinary busy polls, not-yet-due iterations, and audio-priority iterations.

LED completion checks physical DMA/PIO completion before considering the frame
deadline. A delayed poll therefore accepts a frame already physically complete,
starts the mandatory latch interval, and returns to idle after the latch.
Timeout is reported only when the deadline has passed and an active output is
still incomplete; an unconnected strip does not itself constitute a software
error.

The normal-loop priority is fixed: first service an in-progress asynchronous
LED frame; then acquire and process any ready audio block and run its spectrum
analysis; only in an iteration with no ready audio block may the temporary
renderer run; only after another ready-block check may startup delivery or the
one-second bounded telemetry run. Rendering is not performed in the same loop
iteration after audio processing. The temporary diagnostic renderer is limited
to approximately 30 Hz; it remains asynchronous through the existing LED
manager.

## Renderer-only visual normalization

The temporary renderer automatically uses pure display normalization without
changing `AudioLevelFrame` or `SpectrumFrame`:

- audio envelopes are clamped and linearly mapped from provisional 0…2047
  centred-ADC amplitude units to 0…65535 visual units;
- spectrum and macro levels use a clamped square-root visual curve over their
  existing 0…65535 range.

These are named provisional rendering constants and curves, not AGC or DSP
calibration. The temporary six-strip mapping remains unchanged and each pure
renderer primitive still accepts only its caller-provided logical pixel span.
Attack/release tuning is explicitly deferred to Feature 004.

## Definition of done

Software verification requires warning-clean firmware build, host checks of
Q15 reference tolerance, silence, 1/2/4-count floor suppression, 8-count
detection with correct dominant bin and bounded unrelated leakage, signed
symmetry, residual DC removal, reference level, saturation, frequency
classification, continuity, and renderer normalization. Physical acceptance
requires Pico W measurement:

- preferred processing time below 8,000 us and practical hard duration
  comfortably below approximately 16,000 us;
- no normal growth of `adc_drop`, `missing_audio_blocks`, or
  `dropped_windows`;
- no ADC FIFO overflow/underflow;
- correct 80/300/1000/6000 Hz classification;
- automatic visible diagnostic rendering on usable strips.
- `led_frame_timeouts` remains stable during normal operation, while started
  and completed frame counts continue to increase.
- After startup, 60-second quiet-input and ordinary-music runs show no growth
  in `adc_drop`, `missing_audio_blocks`, or `dropped_windows`.
