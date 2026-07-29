# Feature 003 — Spectrum Analysis Physical Validation and Diagnostic Rendering

## Scope

Feature 003 consumes the single shared, read-only `CenteredMonoBlock` produced
by Feature 002. It performs one 1,024-sample FFT per analysis window and
publishes one shared `SpectrumFrame`; diagnostic views do not run independent
FFTs. Aux remains excluded from this FFT. This stage adds physical validation,
raw power diagnostics, a temporary six-strip diagnostic scene, and nonblocking
USB commands. It does not add an Effect Engine, AGC, beat detection, storage,
Wi-Fi, or web configuration.

`SpectrumFrame` contains 32 display bands, Bass/Low/Mid/High, sequence,
analysis timing, dropped-window count, and missing-audio-block count. All
levels are unsigned 0–65535. `SpectrumDiagnostics` contains raw normalized
positive-bin energy, the highest raw bin power, and its bin number. These raw
values are derived from the same completed FFT before floor subtraction,
gain, level conversion, or smoothing.

## FFT contract

Four 256-sample blocks at 32 kHz form the first 1,024-sample window. The next
window retains 512 samples and accepts two further blocks, giving 50% overlap
and roughly a 16 ms output cadence. Feature 002 owns adaptive ADC DC removal;
Feature 003 removes only the arithmetic mean of each completed FFT window
before applying Hann. It therefore does not repeat adaptive DC estimation.

For useful bins 1–384 (31.25 Hz–12 kHz), with `N = 1024` and
`H = mean(hann²)`, normalized positive-bin power is:

```text
power[k] = 2 × (real[k]² + imaginary[k]²) / (N² × H)
```

Band and macro energy is accumulated, never averaged by band width:

```text
E = sum(power[k])
clean_E = max(0, E - noise_floor_per_bin × bin_count)
level = clamp(65535 × sqrt(clean_E × gain / reference_energy), 0, 65535)
```

The provisional constants remain unchanged: floor `1.0` normalized power per
bin, gain `1.0`, reference energy `32768`, attack `1/2`, and release `1/16`.
The reference is a bin-centred sine of approximately 313 centered ADC counts;
its first attack-smoothed result is about 32767. Hardware measurements, not
visual preference, determine future tuning.

## Diagnostic renderer

The renderer is a temporary scene and defaults **off**. Enabling it configures
the existing six tested GRBW SK6812 RGBW outputs at brightness 16 and no more
than 60 frames/s. It uses only the public LED manager logical-pixel API and
skips an update while LED packing, transmission, or latching is active.

The fixed scene is temporary only:

1. Strip 1: 32 bands resampled to 16 contiguous low-to-high spectrum regions,
   with a blue/white → cyan/green → red diagnostic gradient.
2. Strip 2: five weighted-resampled frequency values mirrored about the
   centre; the outside pair is the lowest zone and the centre is the highest.
3. Strip 3: contiguous Bass, Mid, High zones. For non-divisible lengths,
   pixel `i` belongs to `floor(3i / length)`.
4. Strip 4: Feature 002 Left/Right envelopes, centre-out. For odd lengths the
   centre pixel belongs to Left and Right begins one pixel to its right.
5. Strip 5: Feature 002 Mono full-range VU.
6. Strip 6: Feature 002 Aux full-range VU.

Rendering primitives accept an explicit caller-owned logical pixel span and
have no GPIO, PIO, DMA, ADC, or FFT-work-buffer knowledge. Their state is not
globally shared. This preserves the future architecture: each of six strips
may independently choose effect, source, colours, parameters, direction,
geometry, and enabled state, while all consume the same read-only audio and
spectrum frames.

## USB validation controls

Commands are line-based, nonblocking, volatile, and processed only in the
normal loop:

```text
status
stats reset
diagnostics on | diagnostics off
renderer on | renderer off
noise measure | noise cancel
help
```

`noise measure` collects 192 valid FFT windows (about three seconds), skips
windows affected by missing blocks or dropped analysis windows, restarts its
consecutive collection after such a discontinuity, and reports mean raw bin
power, average and worst raw maximum-bin power, and dominant noise region. It
never modifies DSP constants.

## Acceptance and remaining physical validation

Software must build firmware and host-test pure geometry. Physical acceptance
still requires quiet-input measurements, 80/300/1000/6000 Hz tone checks,
reference-level evidence when practical, music, all six views, and zero normal
drop/missing/overflow/underflow counters. Preferred FFT duration is ≤8 ms;
hard maximum is <16 ms. No physical Feature 003 timing, noise, or tone result
is claimed until the project owner supplies measurements.
