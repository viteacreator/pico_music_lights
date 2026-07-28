# Feature 003 — Design

## Backend and ownership

Bring-up uses an in-project radix-2, iterative, single-precision real-input
FFT backend; no external source or licence applies. The backend is isolated
behind `SpectrumAnalyzer`, so it can later be replaced without altering
`SpectrumFrame`, the resampler, or renderer. Input is signed centered `int16_t`;
Hann multiplication and FFT work arrays are `float`; positive-bin power is
`real² + imaginary²`. The measured target is <=8 ms; 16 ms is the hard limit.

The analyzer owns a static 1,024-sample sliding mono buffer. Feature 002
publishes `CenteredMonoBlock { array<int16_t,256> samples; uint32_t sequence; }`
after its `AudioProcessor` has independently centered L/R and formed mono once.
The application owns that block until the analyzer copies it; no analyzer array
is shared with DMA. A sliding copy retains samples 512–1023 after a completed
transform. If a complete next window arrives while analysis is still busy, it
is skipped and `dropped_windows` increments.

Estimated static SRAM: analyzer mono window 2,048 B; one published
`CenteredMonoBlock` 512 B; FFT real 4,096 B; FFT imaginary 4,096 B; 385 powers
1,540 B; raw/smoothed display and macro state under 300 B; 32-element temporary
resampling output 64 B: about 12.4 KiB plus the caller-owned block. Constant
flash tables: Hann 4,096 B, 512 complex twiddles 4,096 B, and 1,024-entry
bit-reversal table 2,048 B, about 10.0 KiB. Tables are `const` and reside in
flash; this avoids SRAM use but flash reads are part of FFT timing. Temporary
stack use is limited to scalar locals (under 256 B); no transform array is on
the stack. Precomputed twiddles avoid per-window sine/cosine work.

## Exact 32-band mapping

Bin spacing is 31.25 Hz. Ranges are inclusive, contiguous, and normalized by
their bin counts. The table is a compile-time constant.

|Band|Bins|Hz approx.|Band|Bins|Hz approx.|
|---:|---:|---:|---:|---:|---:|
|0|1–1|31–31|16|32–36|1000–1125|
|1|2–2|62–62|17|37–42|1156–1313|
|2|3–3|94–94|18|43–49|1344–1531|
|3|4–4|125–125|19|50–58|1563–1813|
|4|5–5|156–156|20|59–68|1844–2125|
|5|6–6|188–188|21|69–80|2156–2500|
|6|7–7|219–219|22|81–94|2531–2938|
|7|8–8|250–250|23|95–110|2969–3438|
|8|9–9|281–281|24|111–129|3469–4031|
|9|10–11|312–344|25|130–151|4063–4719|
|10|12–13|375–406|26|152–177|4750–5531|
|11|14–16|438–500|27|178–207|5563–6469|
|12|17–19|531–594|28|208–242|6500–7563|
|13|20–22|625–688|29|243–283|7594–8844|
|14|23–26|719–813|30|284–331|8875–10344|
|15|27–31|844–969|31|332–384|10375–12000|

## State, compression, and diagnostics

`hann_power_normalization` is the precomputed mean squared Hann coefficient.
Power uses `2*(real²+imaginary²)/(1024²*hann_power_normalization)` before all
aggregation. Bands use accumulated energy `sum(power)` and subtract
`noise_floor_per_bin * bin_count`, not mean power; this prevents equal tones in
wider bands being artificially weakened. Each of 32 display bands and four macro bands has a smoothed state. The global
noise floor, gain, attack, release, and reference power are centralized
provisional constants. Measure current/max analysis duration around transform
and aggregation. Renderer timing is separately rate-limited.
