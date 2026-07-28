# Feature 003 — Spectrum Analysis Bring-Up

## Scope and public contract

The hardware-independent analyzer consumes only centered sample-wise mono:
`(left_centered + right_centered) / 2`. It never accesses ADC, DMA, PIO, GPIO,
LED buffers, Wi-Fi, or storage. Aux is excluded. Feature 002 retains Left,
Right, Aux, and full-range Mono envelopes; Feature 003 does not duplicate them.

```cpp
constexpr size_t kSpectrumBandCount = 32;
struct SpectrumFrame {
    std::array<uint16_t, kSpectrumBandCount> bands;
    uint16_t bass, low, mid, high;
    uint32_t sequence, analysis_time_us, maximum_analysis_time_us, dropped_windows;
};
```

All public spectrum levels are saturated unsigned 16-bit values in 0–65535.

## Window and transform requirements

Input arrives in centered 256-sample mono blocks at 32,000 Hz. Four blocks
form the initial 1,024-sample window. After analysis, the newest 512 samples
are retained and two further blocks produce the next window; output cadence is
approximately 16 ms. Static analyzer-owned storage is used; no heap allocation
is permitted. If an analysis cannot begin before a subsequent complete window
is available, that subsequent window is discarded and `dropped_windows` rises.

Apply precomputed Hann coefficients, execute exactly one 1,024-point real FFT,
ignore DC bin 0 and bins above 384 (12 kHz), calculate power, then derive every
display and macro value from that same power array.

Macro ranges are exact: Bass 1–5 (31.25–156.25 Hz), Low 6–16 (>156.25–500 Hz),
Mid 17–80 (>500–2,500 Hz), High 81–384 (>2,500–12,000 Hz).

## Processing requirements

For each inclusive bin range, use mean power `P = sum(power[i]) / bin_count`.
Use centralized provisional constants: `noise_floor`, `gain`, `attack`, and
`release`. Convert with `level = clamp(65535 * sqrt(max(0, P-noise_floor) *
gain) / reference, 0, 65535)`. Smooth each independent output with
`previous + attack*(level-previous)` while rising, otherwise
`previous + release*(level-previous)`. Fixed-point equivalents may replace
these formulas only if they retain the same contract.

## Resampler and diagnostic renderer

The pure resampler accepts caller-provided storage and maps 32 bands to 5, 8,
16, or 32 segments using weighted source-band overlap, so every source band
contributes even when counts do not divide evenly. It knows no strip geometry.

The separate diagnostic renderer uses public audio, spectrum, and LED APIs:
Strip 1 is a 16-segment spectrum; Strip 2 is five symmetric frequency zones;
Strip 3 is Bass/Mid/High zones; Strip 4 is centre-out L/R VU; Strip 5 is Mono
VU; Strip 6 is Aux VU. It retains GRBW, brightness 16, installed lengths, and
at most about 60 updates/s.

## Verification

Host tests cover window/overlap/Hann behaviour, mapping validity, DC rejection,
tones at 80/300/1000/6000 Hz, mixed tones, monotonic amplitude, opposite-phase
mono cancellation, floor/gain/smoothing, macro aggregation, resampling, and
sequence/drop accounting. Physical testing confirms timing and diagnostic
response with tones and music.
