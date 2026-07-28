# Feature 003 — Design

## Backend and ownership

Bring-up uses an in-project radix-2, iterative, single-precision real-input
FFT backend; no external source or licence applies. The backend is isolated
behind `SpectrumAnalyzer`, so it can later be replaced without altering
`SpectrumFrame`, the resampler, or renderer. Input is signed centered `int16_t`;
Hann multiplication and FFT work arrays are `float`; positive-bin power is
`real² + imaginary²`. The measured target is <=8 ms; 16 ms is the hard limit.

The analyzer owns a static 1,024-sample sliding mono buffer. Normal application
code owns incoming Feature 002 blocks and calls the analyzer after block
processing; ADC DMA interrupt ownership is unchanged. A sliding copy retains
samples 512–1023 after a completed transform. No buffer is shared with DMA.

Estimated static RAM: mono window 2,048 B; Hann coefficients 4,096 B; FFT real
4,096 B; FFT imaginary 4,096 B; 385 powers 1,540 B; raw/smoothed display and
macro state under 300 B; 32-element temporary resampling output 64 B: about
16.3 KiB total.

## Exact 32-band mapping

Bin spacing is 31.25 Hz. Ranges are inclusive, contiguous, and normalized by
their bin counts. The table is a compile-time constant.

|Band|Bins|Hz approx.|Band|Bins|Hz approx.|
|---:|---:|---:|---:|---:|---:|
|0|1–1|31–31|16|52–60|1625–1875|
|1|2–2|62–62|17|61–71|1906–2219|
|2|3–3|94–94|18|72–84|2250–2625|
|3|4–4|125–125|19|85–99|2656–3094|
|4|5–5|156–156|20|100–116|3125–3625|
|5|6–7|188–219|21|117–136|3656–4250|
|6|8–9|250–281|22|137–159|4281–4969|
|7|10–11|312–344|23|160–186|5000–5813|
|8|12–14|375–438|24|187–218|5844–6813|
|9|15–17|469–531|25|219–255|6844–7969|
|10|18–21|562–656|26|256–276|8000–8625|
|11|22–25|688–781|27|277–300|8656–9375|
|12|26–30|812–938|28|301–326|9406–10188|
|13|31–36|969–1125|29|327–353|10219–11031|
|14|37–43|1156–1344|30|354–368|11063–11500|
|15|44–51|1375–1594|31|369–384|11531–12000|

## State, compression, and diagnostics

Each of 32 display bands and four macro bands has a smoothed state. The global
noise floor, gain, attack, release, and reference power are centralized
provisional constants. Measure current/max analysis duration around transform
and aggregation. Renderer timing is separately rate-limited.
