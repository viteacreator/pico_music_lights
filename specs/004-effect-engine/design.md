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

## Gyver VU and reactive rendering corrections

Gyver VU uses raw ADC peaks (`0..2047`) for noise gating. Each independent
side follows this fixed integer pipeline: raw peak, hysteresis gate, safe raw
floor subtraction, normalization by `2047-floor`, attack/release smoothing,
Q8 visual gain, adaptive reference with Q8 headroom, then logical VU length.
Gate closure sets only the smoothing target to zero: the retained reference is
frozen and the bar releases visibly. The reference tracks the ungained
smoothed level, so visual gain changes sensitivity without changing reference
learning. Rise and fall timing are separate per configuration.

Gyver Rainbow derives its outward hue offset from each side's usable maximum
distance and `gyver_rainbow_span_percent`; it subtracts that offset from the
time phase. Generic rainbow keeps per-pixel `color_spacing_q8` semantics.
All reactive colour uses `interpolate_color(background, foreground, level)`.

The USB bring-up parser is bounded, non-blocking, and line based. Calibration
collects peaks during normal audio processing, then uses the existing staged
configuration API. It owns no heap memory and never pauses capture, FFT, or
LED DMA. Telemetry reports raw/effective peaks, gate state, references, floors,
hysteresis and calibration state in a separate bounded line.

Calibrated floors are volatile application-owned values. They are atomically
staged into currently active Gyver VUs and copied into every later diagnostic
Gyver VU scene; calibration never disables diagnostic-scene rotation. The VU
line identifies the first active Gyver VU strip, rather than implying that
strip zero is always a VU.

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
| 6 strobe | timed hard-cut flashes | stroboscope | none | strobe phase | frequency, duty, colour, background |
| 7 ambient | fixed colour, smooth cycle, running rainbow | Static, colour cycle, running rainbow | none | phase | White Boost, speed, spacing, palette |
| 8 running frequencies | moving frequency-coloured trail | running-frequency | macro bands | position, level/decay | selection, speed, decay, direction |
| 9 spectrum | contiguous spectrum analyser | spectrum bars | 32 bands | smoothing | 5/8/16/32 segments, palette, direction |

The strobe is timed and audio-independent. Gyver Stroboscope is always
hard-cut; generic Stroboscope can explicitly choose a fade envelope.
Feature 004 does not copy the original IR handling, EEPROM, or Arduino
hardware loop.

Spectrum bars use the existing weighted 32-band resampling into 5, 8, 16 or 32
segments. Mirrored zones resample to a bounded zone count and map nearest-strip
end toward centre as low-to-high. Macro regions use `floor(region_count * pixel
/ pixel_count)`, so all pixels are assigned for non-divisible lengths. Direction
reverses logical geometry only; Feature 001 physical reversal remains separate.
For three generic Macro Bands regions, `macro_mapping=low_mid_high` maps to
Low/Mid/High and `macro_mapping=bass_mid_high` maps to Bass/Mid/High. The
mapping is a Macro Bands-only metadata parameter; four regions remain the fixed
Bass/Low/Mid/High sequence.

The static future-web contract consists of `EffectMetadata` and
`ParameterDescriptor` tables. Metadata provides stable effect identifier,
display name, category, compatible-source bitmask and public parameter bitmask.
Each descriptor provides a stable identifier, display label, value type, unit,
minimum, maximum, step and canonical default; `effect_parameter_applicability()`
returns the fixed set of effects that advertise the parameter. A separate
12-entry Idle Lighting descriptor table covers every public Idle field or fixed
field group. These tables are compile-time bounded and contain no JSON, heap or
web dependency.

The canonical reset-default scene is constructed once then copied by value into
all six slots: `gyver_vu_gradient`, `stereo_left_right`, Off background, and
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

## Linear VU and global Idle Lighting

`scalar_vu` is the stable internal `linear_vu` effect: one selected Left,
Right, Aux or Mono source grows from one logical end. `reversed` selects the
other end. It supports solid, position-gradient and per-pixel rainbow colour
modes plus RGBW background/palette, Q8 gain, attack/release and animation
parameters. It remains separate from stereo centre-out effects. For odd stereo
VU spans, Left owns the centre pixel and Right starts at the next pixel.

`IdleLightingController` owns one staged global configuration and bounded
runtime. At a renderer frame boundary the engine renders every configured
strip, the controller updates from raw Left/Right/Aux peaks, then blends only
mask-selected strips using `interpolate(idle_rgbw_at_q8_brightness, effect,
effect_mix)`. `effect_mix=0` is full idle and `65535` is full effect. This
does not pause effect state. Independent selected-input gates use raw ADC
floors/hysteresis, then confirmation and silence timers. Defaults are disabled,
but an enabled controller starts idle when `startup_idle_enabled` is true.

`color_spacing_q8` is a Q8 hue increment per logical pixel. Running Rainbow
uses `hue(pixel) = phase + round(pixel * color_spacing_q8 / 256)` with hue
wrap; it never normalizes a cycle to span length. Reversal maps this logical
geometry to the opposite destination order.

`effect_scenes` is a hardware-independent scene-data module. Its named reset
scene returns six independent value copies of the canonical centre-out VU. Its
four 12-second temporary diagnostic scenes cover Gyver and generic VU/ambient,
spectrum/motion catalog members across the six strips. The LED wrapper stages a
due scene atomically at a render boundary; an explicit staged configuration or
reset disables this temporary sequence, so it cannot become a permanent effect
assignment.

## AlexGyver-compatible catalog

The generic extended catalog remains available and names its old circular
moving-frequency effect `frequency_comet`. Gyver-compatible names use the
`gyver_` prefix and are separate `EffectType` values so future web metadata can
distinguish the two families.

| Gyver-compatible effect | Shared input | Geometry / behaviour |
| --- | --- | --- |
| Gyver VU Gradient / Rainbow | Stereo Left/Right | Independent centre-to-end halves, adaptive gain by default |
| Gyver Frequency 5 / 3 Zones | Mono Low/Mid/High | `H,M,L,M,H` or `H,M,L`, adaptive event flashes |
| Gyver Frequency Full Strip | Mono Low/Mid/High | High-first or strongest active event selects Low/Mid/High colour |
| Gyver Running Frequencies | Mono Low/Mid/High | New colour injected at centre, retained half-history moves outward symmetrically |
| Gyver Spectrum Analyzer | 32 spectrum bands | Lowest band at centre, highest at both ends, mirrored palette progression |
| Gyver Stroboscope / Ambient variants | None | RGBW static, time cycle or running rainbow; strobe uses hard-cut duty/background |

For each Low/Mid/High group, the adaptive detector runs once per strip frame:

```text
fast = integer_ramp(fast, input, adaptive_fast_response_ms)
average = integer_ramp(average, fast, adaptive_average_response_ms)
threshold = average * adaptive_trigger_percent / 100
event = fast * visual_gain / 256, if fast > threshold
event = integer_ramp(event, 0, adaptive_event_decay_ms), otherwise
```

The first observed value initializes fast and average without generating an
event. All fields are unsigned 0..65535 except the trigger percentage
(100..1000). This is a provisional RP2040 parameterization of the requested
v2.10 behaviour; exact original threshold/timing constants are not claimed.

`gyver_frequency_full_strip` owns `gyver_full_strip_selection`, exposed as the
metadata parameter `full_strip_policy`; `gyver_running_frequencies` owns the
separate `gyver_running_frequencies_selection`, exposed as `running_policy`.
Both accept `gyver_priority` (High, then Mid, then Low) and `strongest_event`
and default to `gyver_priority`. The separate fields prevent future web code
from inferring effect-specific semantics from a string.

Gyver auto gain retains a per-strip reference for Left, Right and spectrum.
The reference follows the ungained smoothed value using independently
configured rise and slower fall times, but freezes while its VU gate is closed
or all spectrum bands are below the configured floor. Display normalization
uses `level / (reference * headroom_q8 / 256)`, clamped to `0..65535`.
Disabling auto gain passes the gained level through.

Gyver Running Frequencies stores one 150-pixel RGBW logical half per strip, then
mirrors it at render time. This is 600 bytes per strip or 3,600 bytes for six
strips. Odd spans share their centre pixel; even spans use two adjacent centre
pixels. A 1-pixel span is written once, and a zero-length span is rejected
before rendering.

Temporary diagnostic scenes are data-only atomic scenes. Four timed scenes
cover Gyver VU/ambient, Gyver frequency/spectrum, generic spectrum/motion and
the remaining generic ambient/frequency effects. Staging a custom scene or the
named reset-default scene disables this temporary cycle.

Diagnostics retain Feature 003 fields and add `effect_config_generation`,
`effect_frames_started`, `effect_frames_completed`, `effect_frames_skipped_busy`,
`effect_render_us`, `effect_render_max_us`, `effect_frames_failed`, and active
diagnostic scene. The startup report identifies the diagnostic bring-up scene
and the retained reset-default scene. Effect render time and memory are measured in the
Pico firmware/map; host tests validate functional bounds and determinism.

The current ARM Release map reports the six-slot `EffectEngine` global as
5,792 bytes of static SRAM. Its Gyver Running Frequencies half-history accounts
for 3,600 of those bytes across six strips. The current Feature 004 Release
firmware measures 93,996 bytes of text and 20,892 bytes of BSS. The final map
remains authoritative after future changes.
