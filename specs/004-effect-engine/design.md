# Feature 004 — Design

`EffectEngine` owns six fixed `StripEffectRuntime` slots. A runtime contains a
configuration and bounded state. It has a separate pending scene so validation
finishes before the next render boundary; copying the six fixed entries applies
an atomic scene with no heap allocation. A generation counter identifies the
applied configuration.

The application retains the LED runtime as the transport owner. On a due frame
it snapshots the latest public audio and spectrum frames, applies pending
configuration, obtains each strip's logical span, and calls the engine once.
The engine clears disabled/Off spans and dispatches reusable pure renderers.
It then starts one asynchronous all-enabled LED transfer. It never writes while
the LED manager has an active packing/transmission/latch frame.

Effects receive `EffectInputSnapshot`, `EffectRenderSpan`, and their own
runtime. They cannot identify a physical strip. Scalar VU uses unsmoothed audio
peak metrics where available; spectrum and macro effects use Feature 003 raw
levels (`raw_bands`, `raw_bass`, `raw_low`, `raw_mid`, `raw_high`). Existing
Feature 003 smoothed levels remain solely compatible diagnostics.

Per-strip smoothing is a bounded integer first-order ramp. For each output
level, the target is visual-gain scaled and clamped to 0..65535. The change per
frame is proportional to elapsed milliseconds divided by configured attack or
release milliseconds, with a minimum nonzero step while a change remains. This
uses no runtime floating-point exponential calculation and naturally catches up
after skipped frames. Static and Off profiles use zero response time.

`StripEffectConfig` selects `StaticColorMode::direct_rgbw` or
`StaticColorMode::white_boost`. Direct mode transmits the configured RGBW colour
unchanged apart from visual gain. White Boost uses integer rounded formulas:
for `d` in 0..100, `W = round(255*d/100)` and RGB is zero; for `d` in 101..200,
`W = 255` and every assist channel is `round(assist_rgb*(d-100)/100)`. Values
outside 0..200 and assist colours with a nonzero White field are rejected. The
result is calculated before Feature 001 strip brightness and later current
limiting.

The LED runtime wrapper offers controlled application-facing operations to read
one active configuration, stage one configuration, stage a full six-strip scene,
restore the compiled default scene, and read the applied generation. It does
not expose a mutable engine object. `EffectEngine` remains responsible for
validation, pending-scene atomicity and applying at a render boundary.

Only a change that changes state interpretation resets an affected state:
effect type, source, spectrum segment count, mirrored-zone count, macro-region
count, static colour mode, or disabled-to-enabled transition. Colour, gain,
direction, attack and release retain state. Static modes currently maintain no
temporal values, but their mode transition still invokes the same reset policy.

Spectrum bars use the existing weighted 32-band resampling into 5, 8, 16 or 32
segments. Mirrored zones resample to a bounded zone count and map nearest-strip
end toward centre as low-to-high. Macro regions use `floor(region_count * pixel
/ pixel_count)`, so all pixels are assigned for non-divisible lengths. Direction
reverses logical geometry only; Feature 001 physical reversal remains separate.

The default scene is held only as engine configuration: strip 1 spectrum bars
(16); strip 2 mirrored zones (5); strip 3 four macro regions; strip 4 stereo
centre-out; strip 5 Mono VU; strip 6 Aux VU. No reusable effect contains a strip
number.

Diagnostics retain Feature 003 fields and add `effect_config_generation`,
`effect_frames_started`, `effect_frames_completed`, `effect_frames_skipped_busy`,
`effect_render_us`, and `effect_render_max_us`. The startup report prints the
six compiled configurations. Effect render time and memory are measured in the
Pico firmware/map; host tests validate functional bounds and determinism.

On the current ARM Release build, `StripEffectConfig` is 50 bytes,
`StripEffectState` is 80 bytes, `StripEffectRuntime` is 136 bytes, and the
single six-slot engine (active runtimes plus pending scene) is 1,128 bytes of
static SRAM. Raw spectrum publication adds 72 bytes to `SpectrumFrame`.
The current Feature 004 firmware measures 64,356 bytes of text and 15,940
bytes of BSS; compared with the Feature 003 baseline this is approximately
5,412 bytes of flash text and 1,220 bytes of BSS. The final map remains
authoritative after future changes.
