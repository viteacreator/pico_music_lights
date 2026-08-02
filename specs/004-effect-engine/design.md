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

Segmented spectrum, mirrored-zone and macro-band renderers first derive their
bounded set of targets, update each corresponding smoothing slot once, and
cache those results for the following pixel loop. Consequently a given input
has the same temporal response on short and long strips. If integer rounding
would make a nonzero elapsed-time update stall, the value changes by one level
toward its target.

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

## ColorMusic behaviour audit and reusable mapping

The official AlexGyver ColorMusic project page documents modes 1–9 and their
submodes; its linked GitHub repository is the firmware source reference. This
project preserves visible behaviour rather than Arduino/WS2812, IR-remote,
EEPROM, or AVR implementation details.

| Original ColorMusic mode | Observed user-visible behaviour | Reusable effect or parameterization | Required source | Required runtime state | Supported parameters |
| --- | --- | --- | --- | --- | --- |
| 1 VU gradient | loudness bar, green-to-red | Stereo/Scalar VU + gradient | Stereo or scalar | smoothing | gain, attack/release, direction, palette |
| 2 VU rainbow | loudness bar with moving rainbow | VU + animated rainbow | Stereo or scalar | phase | speed, spacing, gain, attack/release |
| 3 five bands | five symmetric frequency zones | mirrored spectrum zones | 32 bands | smoothing | zones, palette, direction |
| 4 three bands | three contiguous frequency regions | macro bands (3) | macro bands | smoothing | palette, direction |
| 5 one band | whole strip follows selected bands | one-band frequency | macro bands | smoothing | three/Low/Mid/High, gain, palette |
| 6 strobe | timed flashes with fade | stroboscope | none | strobe phase/level | frequency, fade, colour |
| 7 ambient | fixed colour, smooth cycle, running rainbow | Static, colour cycle, running rainbow | none | phase | White Boost, speed, spacing, palette |
| 8 running frequencies | moving frequency-coloured trail | running-frequency | macro bands | position, level/decay | selection, speed, decay, direction |
| 9 spectrum | contiguous spectrum analyser | spectrum bars | 32 bands | smoothing | 5/8/16/32 segments, palette, direction |

The strobe is timed and audio-independent, matching its documented controls.
Feature 004 does not copy the original auto-gain, noise calibration, IR
handling, EEPROM, or Arduino hardware loop.

Spectrum bars use the existing weighted 32-band resampling into 5, 8, 16 or 32
segments. Mirrored zones resample to a bounded zone count and map nearest-strip
end toward centre as low-to-high. Macro regions use `floor(region_count * pixel
/ pixel_count)`, so all pixels are assigned for non-divisible lengths. Direction
reverses logical geometry only; Feature 001 physical reversal remains separate.

The canonical default is constructed once then copied by value into all six
slots: `stereo_center_out_vu`, `stereo_left_right`, Off background, and
`level_position_gradient` with Green, Yellow, Orange, Red palette. Each side
uses its distance from logical centre normalized by half capacity to interpolate
the palette; the centre is Green and the end is Red. Odd spans assign the
centre pixel to Left; even spans start Left and Right at adjacent centre pixels.
Default response values are 45 ms attack and 160 ms release, but every strip
remains independently configurable. No reusable effect contains a strip number.

Dynamic state is fixed in each `StripEffectState`: animation phase, strobe
phase/level, Q8 running position, running level, smoothing levels and last
render timestamp. There is no shared mutable effect state. Incompatible
effect/source/geometry/mode/enable transitions reset only their affected strip.

`color_spacing_q8` is a Q8 hue increment per logical pixel. Running Rainbow
uses `hue(pixel) = phase + round(pixel * color_spacing_q8 / 256)` with hue
wrap; it never normalizes a cycle to span length. Reversal maps this logical
geometry to the opposite destination order.

`effect_scenes` is a hardware-independent scene-data module. Its named reset
scene returns six independent value copies of the canonical centre-out VU. Its
two 12-second temporary diagnostic scenes cover VU/ambient and
spectrum/motion catalog members across the six strips. The LED wrapper stages a
due scene atomically at a render boundary; an explicit staged configuration or
reset disables this temporary sequence, so it cannot become a permanent effect
assignment.

Diagnostics retain Feature 003 fields and add `effect_config_generation`,
`effect_frames_started`, `effect_frames_completed`, `effect_frames_skipped_busy`,
`effect_render_us`, `effect_render_max_us`, `effect_frames_failed`, and active
diagnostic scene. The startup report identifies the diagnostic bring-up scene
and the retained reset-default scene. Effect render time and memory are measured in the
Pico firmware/map; host tests validate functional bounds and determinism.

On the current ARM Release build, `StripEffectConfig` is 62 bytes,
`StripEffectState` is 96 bytes, `StripEffectRuntime` is 160 bytes, and the
single six-slot engine (active runtimes plus pending scene) is 1,344 bytes of
static SRAM. Raw spectrum publication adds 72 bytes to `SpectrumFrame`.
The current Feature 004 Release firmware measures 71,380 bytes of text and
16,360 bytes of BSS. The final map remains authoritative after future changes.
