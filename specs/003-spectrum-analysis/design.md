# Feature 003 — Design

## Modules and ownership

`audio_capture` continues to own ADC, DMA, IRQ, and its two buffers.
`audio_processing` produces the centered mono block once. `SpectrumAnalyzer`
is hardware-independent and owns the 1,024-sample overlap window, real and
imaginary arrays, smoothing state, and raw diagnostics. `audio_app` measures
the complete successful analysis call, schedules diagnostics, owns command and
noise-measurement state, and passes immutable published frames to the renderer.

`diagnostic_rendering` is pure: it writes only a supplied `RgbwColor*` span.
`diagnostic_renderer` owns the runtime `LedOutputManager`, the temporary board
configuration, frame scheduling, and renderer counters. It never accesses ADC,
DMA, PIO, or analyzer work arrays. LED transport remains in Feature 001.

## FFT implementation and memory

The 1,024-point radix-2 FFT is in-place over analyzer-owned `real` and
`imaginary` float arrays. Hann is generated per window with a phase recurrence.
Ten constant radix-2 roots are flash-resident; per-stage twiddles are generated
by complex multiplication, bit reversal is algorithmic, and normalized powers
are accumulated directly without a permanent power array. This recurrence is
provisional until measured on Pico W.

Approximate static SRAM: `SpectrumAnalyzer` 10.3 KiB, Feature 002 acquisition
buffers 3,072 B, and Feature 001 logical plus packed LED pools 6,400 B, plus
manager/application objects and SDK state. Constant FFT roots reside in flash.
Temporary renderer stack use is small fixed segment arrays (16 + 5 `uint16_t`)
and no heap allocation is permitted in capture, analysis, commands, or render.
The current ELF map reports `.bss` as `0x4428` (17,448 bytes). The firmware
map remains the authoritative value for total BSS and stack headroom, and this
number can change as future features are added.

## Renderer geometry

Strip 1 uses `resample_spectrum(..., 16)` before stretching each segment across
the caller span. Strip 2 uses `resample_spectrum(..., 5)` and mirrors zones
from the nearest end toward the centre. Thus the weighted resampler owns all
32-to-N source contributions; strip geometry only assigns destination pixels.
Strip 3 uses `floor(3i / count)` for Bass/Mid/High. Strip 4 reserves
`ceil(count/2)` pixels on the logical left for Left and `floor(count/2)` on the
right for Right; each fills from centre outward. Strips 5 and 6 fill from the
logical beginning. Every primitive clears or overwrites every destination pixel
deterministically and bounds all indexes by the supplied span.

## Scheduling and diagnostics

The application invokes `SpectrumAnalyzer::push()` for each processed audio
block. Only a successful output is timed and updates current/max FFT duration.
Raw diagnostic power is calculated while iterating the existing FFT bins,
without another FFT or a power array. Mean useful-bin power is total energy
divided by 384; dominant-bin frequency is `bin × 31.25 Hz`.

Audio capture is initialized first and is mandatory. The LED runtime is then
initialized independently: `ok` and `partial_success` make rendering available,
while a failure is reported and leaves analysis-only validation running. The
renderer first polls the public LED manager. If a frame is active it skips the
update; otherwise, if enabled and due, it writes the six distinct spans and
starts one asynchronous frame. Audio DMA and audio processing continue during
LED DMA. The renderer default is disabled; `renderer on` enables frame
generation only, and `renderer off` leaves the last transmitted frame latched.

The command parser consumes USB bytes with a zero-timeout read into a fixed
48-byte line buffer. An overlong line emits one rejection and enters a discard
state until CR or LF, so no trailing fragment can be parsed as a command. It
makes no interrupt calls, alters no ADC/DMA ownership, and has no persistent
state. Statistics reset is implemented as an application baseline plus
resettable renderer frame counters.
