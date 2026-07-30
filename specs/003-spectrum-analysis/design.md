# Feature 003.1 — Design

## Why Q15 was selected

The previous Release `-O3` Cortex-M0+ build used `-mfloat-abi=soft`. Its
floating-point FFT required roughly 49,500 soft float multiplications, 35,000
soft float additions/subtractions, and 36 square roots per completed window.
Measured analysis time was about 63 ms, far above the 8 ms safe capture
deadline. Precomputed float tables would remove recurrence work but retain soft
float butterfly arithmetic, so they were not a reliable route below 8 ms.

The Q15 backend keeps the existing analysis contract while moving the hot path
to integer arithmetic. The float backend is not built into the Pico production
path. Host tests use a floating direct-DFT reference for Q15 energy comparison;
they do not require Pico hardware headers.

## Analyzer data flow

```text
CenteredMonoBlock (256 Q0 samples)
    → static 1,024-sample overlap window
    → widened residual-mean removal
    → Q15 Hann
    → in-place Q15 FFT, /2 each stage
    → uint64 positive-bin energy
    → 32 display + 4 macro accumulation
    → final normalized level conversion and smoothing
    → SpectrumFrame + SpectrumDiagnostics
```

`SpectrumAnalyzer` owns its sample window, Q15 backend, smoothing state, and
diagnostics. Feature 002 still owns ADC, DMA, IRQ, and two capture buffers.
No capture ownership transition changed.

`ComplexQ15` contains one signed 16-bit real value and one signed 16-bit
imaginary value, so `sizeof(ComplexQ15) == 4`. The 1,024-bin Q15 complex FFT
work array therefore occupies 4,096 bytes SRAM.
The 1,024 Hann coefficients and 512 complex twiddles occupy 4,096 bytes of
flash read-only data. The old float real and imaginary work arrays occupied
8,192 bytes SRAM; the Q15 backend reduces analyzer working memory accordingly.
The current Release map reports 14,444 bytes in `.bss`; the ELF `size` summary
reports 14,636 BSS bytes including its additional allocatable accounting. This
includes the 6,248-byte `SpectrumAnalyzer`, the existing audio/LED state, a
448-byte telemetry buffer, and a 320-byte deferred-startup buffer. The map
remains authoritative because these numbers will change with later features.
The Q15 transform uses scalar locals only and does not put a second 1,024-sample
array on the stack; normal call-stack headroom still requires hardware
validation.

The stage divider rounds signed values to nearest integer with ties-to-even.
Thus `+1` and `-1` both become zero after a divide-by-two stage rather than
persisting as `+1` and `-1`; a half-way value rounds toward the even retained
integer. This avoids systematic one-count residue retention while preserving
symmetry, deterministic operation, saturation protection, and the total
`1 / 1024` transform scale.

Dominant-bin selection is integer-only. A unique nonzero maximum reports its
own bin. For a contiguous equal-energy maximum plateau, the reported bin is
the centre; an even-width plateau uses the lower of its two central bins. If
separated plateaus share the maximum, the first (lowest-frequency) plateau is
selected. Exact silence reports bin 0.

## Runtime scheduling

`audio_app` starts audio first. It then initializes the optional LED runtime;
`ok` and `partial_success` both enable automatic temporary rendering. Failure
does not stop analysis. One shared `SpectrumFrame` feeds all six renderer
views—there is no per-strip FFT.

The normal loop continuously services LED completion, acquires/processes ready
audio blocks, runs the analyzer, then conditionally starts an asynchronous LED
frame. The existing PIO/DMA driver remains unchanged. Telemetry is scheduled
from `time_us_64()`, independent of audio sequences, at one second. The startup
block is prepared in static storage, but delivery is deferred until CDC first
connects and can accept the whole block; this never delays capture or rendering
and occurs once only. Normal telemetry starts after that delivery, uses a fixed
448-byte format buffer and an enlarged 512-byte CDC TX buffer, and is skipped
when the full line cannot be accepted immediately. Raw diagnostic energy is
reported as integer milli-units, avoiding `%f`, `%e`, and `%g` formatting in
the periodic path.

Startup and telemetry use stable formats:

```text
DBG startup build=0.1 audio=ok renderer=ok usable_strips=6 aggregate_hz=96000 mono_hz=32000 fft_size=1024 overlap_pct=50 backend=q15 noise_floor=1 gain=1 reference_energy=32768 renderer_enabled=yes
DBG pixels=132,174,141,81,96,72 order=GRBW brightness=16
DBG t_ms=123456 audio_seq=1000 L=120 R=118 Aux=40 Mono=115 adc_drop=0 adc_over=0 adc_under=0 spectrum_seq=480 bass=12000 low=9000 mid=4000 high=3000 fft_us=4200 fft_max_us=5100 dropped_windows=0 missing_audio_blocks=0 raw_mean_milli=420 raw_max_milli=38100 dominant_bin=3 dominant_hz=94 renderer=ok renderer_frames=600 renderer_skips=0
```

## Renderer normalization

`diagnostic_rendering` remains hardware-independent. It normalizes Feature 002
envelopes before VU geometry and Feature 003 levels before colour scaling.
Normalization is performed once per spectrum segment/zone, not once per pixel,
to keep render cost bounded. The renderer is temporary scene policy only; it
does not establish a permanent strip-to-effect relationship for Feature 004.

## Physical status

Native coverage includes Q15 float-reference tolerance, exact silence,
1/2/4-count floor suppression, 8-count detection/dominant-bin/leakage,
positive/negative symmetry, residual DC removal, reference level, strong-input
safety, and 80/300/1000/6000 Hz classification. The prior float timing failure
is recorded above. Q15 timing, continuity, tone classification, noise
behaviour, USB backpressure behaviour, and visible rendering remain pending
owner hardware measurement.
