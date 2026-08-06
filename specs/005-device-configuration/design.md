# Feature 005 — Device Configuration and Persistent Storage Design

## Overview

Feature 005 introduces a hardware-independent configuration model plus a two-slot binary flash store. It does not add any editor transport. Later Wi-Fi and web-interface features will call the stable configuration service; this feature provides the model, validation, preview/save/reload/reset lifecycle, firmware boot integration points, storage backend abstraction, and tests.

## Module boundaries

- `config/device_config.*`: public fixed limits, enums, value types, and `DeviceConfiguration`.
- `config/factory_defaults.*`: constructs factory defaults from board LED constants and Feature 004 canonical effect defaults.
- `config/config_validation.*`: validates whole configurations and previewable field groups.
- `config/config_service.*`: owns factory, active, draft, and persisted states; exposes preview, activate, save, reload/discard, and factory-reset operations.
- `config/device_config_codec.*`: explicit little-endian binary serialization/deserialization. It never serializes raw C++ object memory.
- `config/crc32.*`: deterministic IEEE CRC32 used by storage records and host tests.
- `storage/persistent_record.*`: record header/trailer definitions, slot inspection, newest-valid selection, and schema policy.
- `storage/flash_backend.*`: small erase/program/read interface with Pico SDK and host fake implementations.
- `storage/device_config_store.*`: coordinates slot writes and reads through a backend.
- Existing runtime adapters in the LED, effects, Idle Lighting, audio, and startup layers consume only validated schema-owned configurations through explicit integration functions that convert to runtime types. Feature 005 reserves eight persisted logical channels but adapts only current board-supported channels 0 through 5 to runtime; it does not add LED transport or hardware support for channels 6 and 7.

Effects remain hardware-independent and never access storage or flash. LED output remains independent from audio, effects, web, Wi-Fi, and persistent storage internals. Storage never manipulates effect state directly and never stores runtime effect or Idle C++ structs; it stores only validated schema-owned configuration values.

## Public interfaces

The implementation shall define fixed-capacity C++17 interfaces equivalent to:

```cpp
namespace config {
constexpr std::size_t kMaximumLedChannelCount = 8;
constexpr std::size_t kCurrentBoardSupportedLedChannelCount = board::kStripCount;
constexpr uint16_t kPersistentPixelCountRepresentationMaximum = UINT16_MAX;
constexpr uint16_t kCurrentBoardMaximumPixelsPerChannel = board::kMaxPixelsPerStrip;
constexpr uint16_t kCurrentBoardMaximumTotalPixels = board::kMaxConfiguredPixels;
// Board/runtime capabilities are validation policy, not schema fields.
constexpr uint32_t kCurrentSchemaVersion = 1;

struct PhysicalStripMetadata {
    // Informational only. Zero means unknown or not measured.
    // Millimetre precision is sufficient; micrometre precision is not used.
    uint16_t length_mm;
    // Zero means unknown. Any nonzero uint16_t value is representable informational metadata.
    uint16_t density_pixels_per_metre;
};

struct LedChannelDeviceConfig {
    bool enabled;
    uint16_t pixel_count;
    uint8_t brightness;
    ChannelOrder channel_order;
    bool reversed;
    PhysicalStripMetadata physical;
};

struct AudioCalibrationConfig {
    uint16_t gyver_left_noise_floor;
    uint16_t gyver_right_noise_floor;
    uint16_t gyver_noise_gate_hysteresis;
    uint16_t gyver_spectrum_noise_floor;
    uint16_t gyver_spectrum_minimum_peak;
};

// Canonical device-level owner for Gyver calibration. These values are
// serialized exactly once in the global audio-calibration record. The same
// fields currently present in effects::StripEffectConfig are runtime mirrors
// only and are excluded from persisted per-channel effect records. Idle Lighting
// activity floors and hysteresis remain owned by effects::IdleLightingConfig
// and are serialized in the Idle Lighting record: left_activity_floor,
// right_activity_floor, aux_activity_floor, and activity_hysteresis.

struct EffectDeviceConfig {
    // Schema-owned logical fields represented by the 106-byte effect record.
    // This is not effects::StripEffectConfig and contains no runtime state or
    // Gyver calibration mirror fields.
};

struct IdleLightingDeviceConfig {
    // Schema-owned logical fields represented by the 30-byte Idle Lighting
    // record, including an eight-bit logical-channel mask. This is not raw
    // effects::IdleLightingConfig storage.
};

struct DeviceConfiguration {
    std::array<LedChannelDeviceConfig, kMaximumLedChannelCount> led_channels;
    std::array<EffectDeviceConfig, kMaximumLedChannelCount> effects;
    IdleLightingDeviceConfig idle_lighting;
    AudioCalibrationConfig audio_calibration;
};
}
```

Result types shall be bounded enums with detail fields such as failing section, logical channel index, field id, slot id, and fallback reason. Diagnostics store only the latest bounded summaries and counters, not unbounded logs. Save coordination shall expose compile-time constants for LED-safe-point and audio-safe-point deadlines so timeout paths are deterministic and host-testable. Logical channel index, not GPIO, is the stable persisted identity; GPIO is resolved only through board configuration.

## Configuration state ownership

- Factory defaults: owned by `factory_defaults`; immutable value construction, no flash dependency.
- Active runtime configuration: owned by `ConfigService` and published by value to runtime adapters only at safe boundaries.
- Draft configuration: owned by `ConfigService`; edited through validated setters or whole-draft replacement.
- Last successfully persisted configuration: owned by `ConfigService`; updated only after a save or factory-reset write commits successfully, or after boot loads a valid supported record.

Dirty state is true when the draft differs from the last persisted configuration, or when active preview differs from last persisted after a successful preview but before Save. Equality is deterministic field-by-field comparison, not byte comparison of structs. Equality and dirty-state checks operate on schema-owned `config::EffectDeviceConfig` and `config::IdleLightingDeviceConfig` values, not runtime effect/Idle structs. They ignore duplicated Gyver calibration fields inside runtime `StripEffectConfig` mirrors and compare only `DeviceConfiguration::audio_calibration` for those values.

## Validation flow

Field preview validation runs first and rejects invalid enum values, incompatible effect/source pairs, out-of-range effect parameters, brightness outside 0..255, Idle bounds, and safe Gyver effect-threshold bounds. Whole-configuration validation then checks the fixed eight-record channel capacity, current board-supported channel count, enabled-channel support, enabled-channel pixel counts, disabled-channel stored pixel counts, current board/runtime per-channel and total enabled pixel capability, physical metadata ranges, serialization-size limits, and all integer overflow risks. An enabled board-supported channel must have `pixel_count >= 1` and must not exceed `board::kMaxPixelsPerStrip`, currently 300. A disabled or unsupported channel may store any schema-valid `uint16_t` pixel count up to `UINT16_MAX`; enabling it later requires validation against the then-current board per-channel and total capabilities. Disabled and unsupported channels do not contribute to the current total enabled-pixel limit. When a disabled channel is adapted to the current LED runtime, the adapter publishes `enabled=false`, runtime `pixel_count=0`, no pixel-buffer slice, and no transmission; the schema-owned retained persistent `pixel_count` is unchanged. Enabling that channel later validates the retained count against current per-channel and total runtime limits before publication. Unsupported board channels, currently logical channels 6 and 7, must remain disabled. Runtime adapters publish only board-supported logical channels, currently channels 0..5, to the six-channel LED driver and EffectEngine. Schema compatibility with eight records does not prove that the current LED transport can drive eight channels. Future activation of channels 6 and 7 is outside Feature 005 and requires an approved hardware/runtime task with an explicit PIO/DMA resource design compatible with Pico W networking. Future board support may raise the board-supported channel count up to the fixed eight-channel persistent capacity without a schema migration solely for those planned channels. `pixel_count` is the sole operational rendering/transmission authority. `length_mm` and `density_pixels_per_metre` are informational only; changing either never recalculates or mutates `pixel_count`. `length_mm == 0` means unknown or not measured and is valid; the maximum representable length is 65,535 mm. Millimetre precision is sufficient and micrometre precision is not used. `density_pixels_per_metre == 0` means unknown; any nonzero `uint16_t` density value is representable informational metadata. Approximate or unknown physical metadata must not make validation reject an otherwise valid authoritative pixel count. GPIO is not present in the persisted model and is not loaded, serialized, validated as a stored value, or compared for dirty state.

Persistent validation additionally checks schema version, payload length, CRC32, commit page, slot alignment, erase/program alignment, slot bounds, and application-image overlap. The persistent Idle Lighting channel mask is an unsigned 8-bit logical-channel mask: bit 0 is logical LED channel 0 and bit 7 is logical LED channel 7. Current board validation permits bits 0..5 only and rejects bits 6 and 7. The current runtime adapter passes only supported-channel bits to `effects::IdleLightingConfig`; future eight-channel board and EffectEngine support may make bits 6 and 7 valid without schema migration. Future schema versions are classified as unsupported and are never interpreted as current payloads.

## Preview and activation flow

Previewable non-structural changes are effects, colours, visual parameters, strip brightness, Idle Lighting settings, and safe Gyver effect-threshold calibration values. Effect, Idle, and Gyver threshold changes may be atomically published at the existing render boundary: the service validates the draft subset and stages effect and Idle configs through their existing staged APIs. Before staging any applicable Gyver runtime effect configuration, a configuration adapter overlays `DeviceConfiguration::audio_calibration` into the runtime mirror fields currently present in `effects::StripEffectConfig`; those values are consumed by the EffectEngine/effect renderer. Reading active or draft configuration normalizes those mirrored fields from the canonical global calibration, so contradictory per-channel calibration state cannot be observed through Feature 005 public APIs or serialized. Feature 005 does not add or require a new audio-processing calibration publication path for ordinary preview of these Gyver effect thresholds. The audio safe point belongs to controlled flash Save/reset coordination, not to ordinary preview publication. If any affected layer rejects the staged value, the whole publication fails and the active configuration is unchanged. The configuration generation increments only after all affected layers accept the change.

Structural LED changes are enabled state, pixel count, channel order, reversal, and equivalent output-layout changes. They are accepted into the draft only after validation but are not applied mid-frame. Activation is a two-phase operation: first validate and precompute every required fixed-capacity slice, span, and hardware-resource plan without touching live output; then switch at a controlled LED-output reinitialization boundary or controlled restart. At that boundary, rendering is paused, any in-flight LED frame reaches idle/latch completion or times out safely, existing initialized LED drivers and already claimed PIO state machines/DMA channels are reused, effect spans are rebuilt, and rendering resumes. Structural activation must not attempt to reclaim resources already owned by an initialized LED driver. A newly enabled board-supported channel may claim its resources once; resource-claim failure leaves the previous active configuration unchanged. Disabling a channel may retain already claimed resources until reboot unless a separately verified rollback-safe release path is implemented. Repeated enable/disable/save operations must not leak resources or cause repeated initialization failures. If hardware reinitialization partially fails and rollback is not possible, LEDs enter a safe-off or prior-safe-output state, active configuration remains reported as the previous configuration, dirty remains true, and diagnostics identify the failed phase and channel/resource.

Publication is all-or-nothing. If any layer rejects a validated publication, no new active configuration is reported; diagnostics record the layer and reason, and the draft remains available for correction or discard.

## Save, reload, and reset semantics

Save follows one ordered state machine: (1) validate and canonicalize the draft without changing active runtime state; (2) serialize the canonical payload into a fixed RAM buffer without changing active runtime state; (3) inspect slots and choose the target using safe read-only operations; (4) acquire the bounded LED safe point; (5) acquire the bounded audio safe point; (6) perform required controlled activation only after both safe points are acquired; (7) erase the target slot; (8) program header and payload data pages while the commit page remains erased; (9) read back and verify the uncommitted header and payload; (10) program the complete 256-byte commit page; (11) read back and perform full committed-record validation; (12) update last-persisted state and clear dirty only after complete validation; and (13) restore audio/rendering and finalize diagnostics. LED or audio safe-point timeout occurs before activation and before erase/program/commit, leaves active, draft, and persisted states exactly as they were before Save except diagnostics, and preserves dirty state. Activation failure issues no erase/program/commit and leaves active and last-persisted unchanged. A failure before commit-page programming leaves the target uncommitted. A failure during commit-page programming may leave either a valid committed target or an invalid uncommitted target; boot validation decides solely from exact record and commit-page validation. Flash or verification failure after successful activation may leave the new configuration active while persisted remains old and dirty remains true; Reload/discard remains the explicit rollback mechanism after such a failure.

Reload/discard replaces draft with last-persisted configuration, or factory defaults if no persisted configuration exists. If preview had changed active non-structural values, reload republishes the restored values at a safe boundary. Structural rollback follows the same controlled reinitialization/restart rule.

Factory reset requires a confirmation token or equivalent explicit two-step API that cannot be triggered by malformed draft data. Confirmed reset uses the same ordered transaction with factory defaults as the canonical configuration: construct and serialize defaults, inspect slots, acquire LED and audio safe points, activate defaults only after both safe points, erase the target slot, program header and payload while the commit page remains erased, verify the uncommitted data region, program the complete commit page, and then verify the committed record. Safe-point timeout or activation failure issues no erase/program/commit. If flash write or verification fails after default activation, factory defaults may remain active while persisted remains old and dirty remains true; diagnostics must distinguish active factory fallback from persisted reset success.

## Binary payload format

All multibyte fields are little-endian unsigned integers. Boolean fields are one byte `0` or `1`; any other value is invalid. Enums are stored from schema-owned config enums using the schema-1 numeric ids below, never as implicit C++ enum ordinals from runtime or configuration C++ types. Reserved bytes are written as zero and must be zero for schema 1 unless another value is explicitly listed in a table. No table below depends on C++ structure padding or object representation.

### Stable schema-1 enum ids

| Enum | Stored ids | Validation |
| --- | --- | --- |
| `ChannelOrder` | `0=rgbw`, `1=grbw` | Other values invalid. |
| `EffectType` | `0=off`, `1=static_rgbw`, `2=scalar_vu`, `3=stereo_center_out_vu`, `4=spectrum_bars`, `5=mirrored_spectrum_zones`, `6=macro_bands`, `7=one_band_frequency`, `8=stroboscope`, `9=ambient_color_cycle`, `10=running_rainbow`, `11=frequency_comet`, `12=gyver_vu_gradient`, `13=gyver_vu_rainbow`, `14=gyver_frequency_5_zones`, `15=gyver_frequency_3_zones`, `16=gyver_frequency_full_strip`, `17=gyver_stroboscope`, `18=gyver_ambient_static`, `19=gyver_ambient_color_cycle`, `20=gyver_ambient_running_rainbow`, `21=gyver_running_frequencies`, `22=gyver_spectrum_analyzer` | Other values invalid or unsupported. |
| `EffectSource` | `0=none`, `1=left`, `2=right`, `3=aux`, `4=mono`, `5=bass`, `6=low`, `7=mid`, `8=high`, `9=stereo_left_right`, `10=spectrum_32`, `11=macro_bands` | Other values invalid; incompatible source/effect pairs invalid. |
| `VuColorMode` | `0=solid`, `1=level_position_gradient`, `2=animated_rainbow` | Other values invalid. |
| `StaticColorMode` | `0=direct_rgbw`, `1=white_boost` | Other values invalid. |
| `FrequencySelection` | `0=three_frequencies`, `1=low`, `2=mid`, `3=high` | Other values invalid. |
| `GyverFullStripSelectionPolicy` and `GyverRunningFrequenciesSelectionPolicy` | `0=gyver_priority`, `1=strongest_event` | The two fields are separate stored fields even though they share ids; other values invalid. |
| `GenericMacroBandMapping` | `0=low_mid_high`, `1=bass_mid_high` | Other values invalid. |
| `StrobeEnvelopeMode` | `0=hard_cut`, `1=fade_envelope` | Other values invalid. |

### Schema-1 payload layout

The complete schema-1 payload before padding is exactly 978 bytes. Payload padding is not part of `payload_length` and is excluded from CRCs. The first four bytes are payload control fields.

| Offset | Width | Field | Representation/endian | Valid values | Reserved values | Validation rule |
| ---: | ---: | --- | --- | --- | --- | --- |
| 0 | 2 | `payload_schema_minor` | `uint16_le` | `0` | `1..65535` | Nonzero is unsupported for schema 1. |
| 2 | 2 | `payload_flags_reserved` | `uint16_le` | `0` | `1..65535` | Must be zero. |
| 4 | 80 | `led_channels[8]` | 8 × 10-byte LED-channel record | See LED table | None | Exactly eight records keyed by logical index. |
| 84 | 848 | `effects[8]` | 8 × 106-byte effect record | See effect table | None | Exactly eight records keyed by logical index. |
| 932 | 30 | `idle_lighting` | 30-byte Idle record | See Idle table | None | Stable config-layer adapter, not runtime struct bytes. |
| 962 | 16 | `audio_calibration` | 16-byte audio record | See audio table | None | Canonical global Gyver calibration serialized once. |

### LED-channel record, 10 bytes

| Offset | Width | Field | Representation/endian | Valid values | Reserved values | Validation rule |
| ---: | ---: | --- | --- | --- | --- | --- |
| 0 | 1 | `enabled` | `uint8` bool | `0`, `1` | `2..255` | Unsupported board channels must be `0`; enabled board channels require valid pixels. |
| 1 | 2 | `pixel_count` | `uint16_le` | `0..65535` | None | Enabled board channels require `1..board::kMaxPixelsPerStrip`; disabled/unsupported may retain any value. |
| 3 | 1 | `channel_order` | `uint8` enum | `0=rgbw`, `1=grbw` | `2..255` | Reserved channels 6 and 7 factory value is `1=grbw`. |
| 4 | 1 | `reversed` | `uint8` bool | `0`, `1` | `2..255` | Structural activation boundary required for changes. |
| 5 | 1 | `brightness` | `uint8` | `0..255` | None | Factory value is 16 for all eight channels. |
| 6 | 2 | `length_mm` | `uint16_le` | `0..65535` | None | `0` means unknown; informational only. |
| 8 | 2 | `density_pixels_per_metre` | `uint16_le` | `0..65535` | None | `0` means unknown; informational only; does not allocate or recalculate pixels. |

### Effect record, 106 bytes

All RGBW colour fields are serialized in logical RGBW byte order: red, green, blue, white. Non-applicable fields are canonicalized to the defaults listed below during serialization; a deserializer accepts a valid record with non-applicable defaults only, so contradictory hidden configuration does not round-trip. Gyver VU/spectrum calibration mirror fields (`gyver_left_noise_floor`, `gyver_right_noise_floor`, `gyver_noise_gate_hysteresis`, `gyver_spectrum_noise_floor`, `gyver_spectrum_minimum_peak`) and all runtime state fields (`StripEffectState`, animation phases, smoothing buffers, gates, histories, timestamps, positions, and sequence counters) are never persisted.

| Offset | Width | Field | Representation/endian | Valid values | Reserved/default values | Validation rule |
| ---: | ---: | --- | --- | --- | --- | --- |
| 0 | 1 | `enabled` | `uint8` bool | `0`, `1` | `0` for Effect Off | Must be compatible with `effect_type`. |
| 1 | 1 | `effect_type` | `uint8` enum | EffectType ids 0..22 | 23..255 reserved | Unknown ids rejected. |
| 2 | 1 | `source` | `uint8` enum | EffectSource ids 0..11 | 12..255 reserved | Must be compatible with effect; `0` for Effect Off. |
| 3 | 4 | `primary_color_rgbw` | 4 bytes | any bytes | `{0,0,0,0}` when not applicable | Valid colour bytes. |
| 7 | 4 | `secondary_color_rgbw` | 4 bytes | any bytes | `{0,0,0,0}` when not applicable | Valid colour bytes. |
| 11 | 4 | `background_color_rgbw` | 4 bytes | any bytes | `{0,0,0,0}` when not applicable | Valid colour bytes. |
| 15 | 16 | `palette_rgbw[4]` | 4 × 4 bytes | any bytes | all zero when not applicable | Gyver VU Gradient factory palette is Green/Yellow/Orange/Red. |
| 31 | 1 | `static_color_mode` | `uint8` enum | 0..1 | `0` when not applicable | Unknown ids rejected. |
| 32 | 2 | `white_drive_percent` | `uint16_le` | `0..200` | `100` when not applicable | Feature 004 bounds. |
| 34 | 4 | `rgb_assist_color_rgbw` | 4 bytes | RGB any, white must be 0 | `{255,255,255,0}` when not applicable | White Boost assist white byte must be zero. |
| 38 | 12 | `gyver_frequency_colors[3]` | 3 × 4 bytes | any bytes | Red/Green/Yellow defaults when not applicable | Low/Mid/High order. |
| 50 | 1 | `vu_color_mode` | `uint8` enum | 0..2 | `0` when not applicable | Unknown ids rejected. |
| 51 | 1 | `frequency_selection` | `uint8` enum | 0..3 | `0` when not applicable | Unknown ids rejected. |
| 52 | 1 | `gyver_full_strip_selection` | `uint8` enum | 0..1 | `0` when not applicable | Unknown ids rejected. |
| 53 | 1 | `gyver_running_frequencies_selection` | `uint8` enum | 0..1 | `0` when not applicable | Unknown ids rejected. |
| 54 | 1 | `macro_band_mapping` | `uint8` enum | 0..1 | `0` when not applicable | Unknown ids rejected. |
| 55 | 2 | `animation_speed_q8` | `uint16_le` | `0..4096` | `256` when not applicable | Current Feature 004 maximum. |
| 57 | 2 | `color_spacing_q8` | `uint16_le` | `0..4096` | `256` when not applicable | Current Feature 004 maximum. |
| 59 | 2 | `fade_decay_ms` | `uint16_le` | `0..5000` | `180` when not applicable | Current Feature 004 response bound. |
| 61 | 1 | `strobe_frequency_hz` | `uint8` | `1..30` when effect is stroboscope, otherwise canonical `8` | `8` when not applicable | Current Feature 004 strobe bound. |
| 62 | 1 | `strobe_duty_percent` | `uint8` | `0..100` | `50` when not applicable | Bounds validated. |
| 63 | 2 | `strobe_fade_ms` | `uint16_le` | `0..5000` | `40` when not applicable | Current Feature 004 response bound. |
| 65 | 1 | `strobe_envelope_mode` | `uint8` enum | 0..1 | `0` when not applicable | Unknown ids rejected. |
| 66 | 2 | `background_brightness_q8` | `uint16_le` | `0..1024` | `0` when not applicable | Current Feature 004 gain bound. |
| 68 | 1 | `auto_gain_enabled` | `uint8` bool | `0`, `1` | `1` when not applicable | Non-bool rejected. |
| 69 | 2 | `auto_gain_headroom_q8` | `uint16_le` | `256..1024` | `461` when not applicable | Current Feature 004 headroom bound. |
| 71 | 2 | `adaptive_fast_response_ms` | `uint16_le` | `0..5000` | `40` when not applicable | Current Feature 004 response bound. |
| 73 | 2 | `adaptive_average_response_ms` | `uint16_le` | `0..5000` | `700` when not applicable | Current Feature 004 response bound. |
| 75 | 2 | `adaptive_trigger_percent` | `uint16_le` | `100..1000` | `125` when not applicable | Current Feature 004 adaptive trigger bound. |
| 77 | 2 | `adaptive_event_decay_ms` | `uint16_le` | `0..5000` | `180` when not applicable | Current Feature 004 response bound. |
| 79 | 2 | `gyver_animation_interval_ms` | `uint16_le` | `0..5000` | `33` when not applicable | Current Feature 004 response bound. |
| 81 | 2 | `gyver_rainbow_span_percent` | `uint16_le` | `0..400` | `50` when not applicable | Current Feature 004 rainbow-span bound. |
| 83 | 2 | `auto_gain_reference_rise_ms` | `uint16_le` | `0..5000` | `300` when not applicable | Current Feature 004 response bound. |
| 85 | 2 | `auto_gain_reference_fall_ms` | `uint16_le` | `0..5000` | `1800` when not applicable | Current Feature 004 response bound. |
| 87 | 1 | `frequency_comet_tail_percent` | `uint8` | `0..100` | `20` when not applicable | Bounds validated. |
| 88 | 2 | `frequency_comet_quiet_threshold` | `uint16_le` | `0..65535` | `256` when not applicable | Source threshold. |
| 90 | 1 | `reversed` | `uint8` bool | `0`, `1` | `0` when not applicable | Non-bool rejected. |
| 91 | 2 | `visual_gain` | `uint16_le` | `0..1024` | `256` when not applicable | Bounds validated. |
| 93 | 2 | `attack_ms` | `uint16_le` | `0..5000` | `0` when not applicable; factory Gyver VU uses 45 | Bounds validated. |
| 95 | 2 | `release_ms` | `uint16_le` | `0..5000` | `0` when not applicable; factory Gyver VU uses 160 | Bounds validated. |
| 97 | 1 | `segment_count` | `uint8` | `5`, `8`, `16`, `32` | `16` when not applicable | Current Feature 004 supported segment counts. |
| 98 | 1 | `zone_count` | `uint8` | `1..16` | `5` when not applicable | Current Feature 004 mirrored-zone bound. |
| 99 | 1 | `macro_region_count` | `uint8` | `3`, `4` | `4` when not applicable | Current Feature 004 macro-region counts. |
| 100 | 6 | `effect_reserved` | bytes | all zero | nonzero reserved | Must be zero for schema 1. |

### Idle Lighting record, 30 bytes

| Offset | Width | Field | Representation/endian | Valid values | Reserved values | Validation rule |
| ---: | ---: | --- | --- | --- | --- | --- |
| 0 | 1 | `enabled` | `uint8` bool | `0`, `1` | `2..255` | Factory default is disabled. |
| 1 | 1 | `startup_idle_enabled` | `uint8` bool | `0`, `1` | `2..255` | Runtime adapter field. |
| 2 | 2 | `silence_timeout_ms` | `uint16_le` | `0..60000` | None | Current Feature 004 Idle timing bound. |
| 4 | 2 | `audio_confirmation_ms` | `uint16_le` | `0..60000` | None | Current Feature 004 Idle timing bound. |
| 6 | 4 | `idle_color_rgbw` | 4 bytes | any bytes | None | RGBW bytes. |
| 10 | 2 | `idle_brightness_q8` | `uint16_le` | `0..256` | None | Values above 256 invalid. |
| 12 | 2 | `fade_to_effect_ms` | `uint16_le` | `0..60000` | None | Current Feature 004 Idle timing bound. |
| 14 | 2 | `fade_to_idle_ms` | `uint16_le` | `0..60000` | None | Current Feature 004 Idle timing bound. |
| 16 | 1 | `activity_input_mask` | `uint8` bitmask | Current input bits only | Unsupported bits | Unsupported input bits invalid. |
| 17 | 1 | `logical_channel_mask` | `uint8` bitmask | Current board bits 0..5; future board may allow 0..7 | Current schema reserves none; current board rejects bits 6..7 | Stable persisted mask; adapter maps to current runtime. |
| 18 | 2 | `left_activity_floor` | `uint16_le` | `0..2047` | None | Owned by Idle Lighting, not audio calibration. |
| 20 | 2 | `right_activity_floor` | `uint16_le` | `0..2047` | None | Owned by Idle Lighting. |
| 22 | 2 | `aux_activity_floor` | `uint16_le` | `0..2047` | None | Owned by Idle Lighting. |
| 24 | 2 | `activity_hysteresis` | `uint16_le` | `0..2047` | None | Owned by Idle Lighting. |
| 26 | 4 | `idle_reserved` | bytes | all zero | nonzero reserved | Must be zero for schema 1. |

### Global audio-calibration record, 16 bytes

| Offset | Width | Field | Representation/endian | Valid values | Reserved values | Validation rule |
| ---: | ---: | --- | --- | --- | --- | --- |
| 0 | 2 | `gyver_left_noise_floor` | `uint16_le` | `0..2047` | None | Canonical Gyver VU raw peak threshold. |
| 2 | 2 | `gyver_right_noise_floor` | `uint16_le` | `0..2047` | None | Canonical Gyver VU raw peak threshold. |
| 4 | 2 | `gyver_noise_gate_hysteresis` | `uint16_le` | `0..2047` | None | Canonical Gyver VU raw hysteresis. |
| 6 | 2 | `gyver_spectrum_noise_floor` | `uint16_le` | `0..65535` | None | Canonical Gyver Spectrum threshold. |
| 8 | 2 | `gyver_spectrum_minimum_peak` | `uint16_le` | `0..65535` | None | Canonical Gyver Spectrum threshold. |
| 10 | 6 | `audio_reserved` | bytes | all zero | nonzero reserved | Must be zero for schema 1. |

## Record and slot format

Persistent flash constants for schema 1 are exact: record magic bytes are `50 4D 4C 43` (ASCII `PMLC`; little-endian numeric interpretation `0x434C4D50` if read as `uint32_le`), commit magic bytes are `43 4D 54 31` (ASCII `CMT1`; little-endian numeric interpretation `0x31544D43` if read as `uint32_le`), `record_format_version=1`, `payload_schema_version=1`, header size `64` bytes, program page size `256` bytes, slot size `4096` bytes, data region size `3840` bytes, commit page offset `3840`, and commit page size `256` bytes. Every persistent slot is erase-sector aligned. Every slot size is a multiple of both the flash erase-sector size and flash program-page size. Header and payload occupy only the data region before the commit page. The final 256-byte page of each slot is the dedicated commit page.

### Record header, 64 bytes

| Offset | Width | Field | Representation/endian | Valid values | Reserved values | Validation rule |
| ---: | ---: | --- | --- | --- | --- | --- |
| 0 | 4 | `record_magic` | bytes | `50 4D 4C 43` | any other | Exact byte match required. |
| 4 | 2 | `record_format_version` | `uint16_le` | `1` | all other values | Unsupported values invalid. |
| 6 | 2 | `payload_schema_version` | `uint16_le` | `1` | all other values | Unsupported values invalid. |
| 8 | 2 | `header_length` | `uint16_le` | `64` | all other values | Exact size required. |
| 10 | 2 | `header_reserved0` | `uint16_le` | `0` | nonzero | Included in header CRC and must be zero. |
| 12 | 4 | `payload_length` | `uint32_le` | `978` for schema 1 | all other values | Must equal complete schema-1 payload length and fit data region. |
| 16 | 4 | `sequence` | `uint32_le` | `0..0xFFFFFFFF` | None | Used by slot selection. |
| 20 | 4 | `payload_crc32` | `uint32_le` | CRC-32/ISO-HDLC of payload bytes | None | Covers exactly `payload_length` bytes. |
| 24 | 4 | `data_region_length` | `uint32_le` | `3840` | all other values | Must equal slot size minus commit page size. |
| 28 | 4 | `slot_size` | `uint32_le` | `4096` | all other values | Must equal schema-1 slot size. |
| 32 | 4 | `commit_page_offset` | `uint32_le` | `3840` | all other values | Must point to final page. |
| 36 | 20 | `header_reserved1` | bytes | all zero | nonzero | Included in header CRC and must be zero. |
| 56 | 4 | `header_crc32` | `uint32_le` | CRC-32/ISO-HDLC over header bytes 0..55 followed by bytes 60..63 | None | Offsets 56..59 containing this field are omitted; no C++ padding participates. |
| 60 | 4 | `header_reserved2` | bytes | all zero | nonzero | Included in header CRC coverage before the CRC field is checked. |

Header CRC32 uses the same CRC-32/ISO-HDLC variant as payload CRC. It covers exactly header byte offsets 0 through 55 followed by offsets 60 through 63 in stored order; it omits offsets 56 through 59 containing `header_crc32`. The commit page is excluded. All reserved header bytes are included in the CRC coverage and must also be zero; nonzero reserved bytes invalidate the record even if a recomputed CRC would otherwise match.

### Commit page, 256 bytes

| Offset | Width | Field | Representation/endian | Valid values | Reserved values | Validation rule |
| ---: | ---: | --- | --- | --- | --- | --- |
| 0 | 4 | `commit_magic` | bytes | `43 4D 54 31` | any other | Exact byte match required for committed record. |
| 4 | 4 | `commit_sequence` | `uint32_le` | equals header `sequence` | mismatch | Must match header. |
| 8 | 4 | `commit_payload_crc32` | `uint32_le` | equals header `payload_crc32` | mismatch | Must match header. |
| 12 | 4 | `commit_payload_length` | `uint32_le` | equals header `payload_length` | mismatch | Must match header. |
| 16 | 240 | `commit_unused_erased` | bytes | all `0xFF` | any non-`0xFF` byte | Exact erased-fill required. |

The commit page remains entirely erased (`0xFF`) while the target header and payload are written and pre-commit verified. The complete 256-byte commit page is programmed as the final authority-changing flash operation; no requirement assumes that a four-byte flash program operation is available. The page-program source buffer contains bytes 0..15 as defined above and bytes 16..255 as `0xFF`, so unused commit-page bytes remain erased. Boot validation requires the exact commit-page contents. An erased, partially programmed, malformed, or inconsistent commit page is uncommitted and invalid. If power fails after sufficient commit-page bytes have been fully programmed and all header/payload integrity checks pass, the new slot may correctly be considered committed. Interrupted-program tests must simulate interruption at every relevant position in the 256-byte commit-page program operation.

### CRC variant and coverage

Header and payload CRC fields use CRC-32/ISO-HDLC: reflected polynomial `0xEDB88320`, initial value `0xFFFFFFFF`, reflected input and output, final XOR `0xFFFFFFFF`, and ASCII check vector `123456789` equals `0xCBF43926`. The payload CRC covers exactly `payload_length` canonical payload bytes and excludes erased data-region padding, header bytes, and the commit page. Header CRC coverage is defined above and excludes the commit page. CRC validation never depends on C++ structure padding.

## Slot-selection algorithm

At boot and after writes, inspect both slots independently:

1. erased or missing slot -> invalid with erased/missing reason;
2. magic/header/header CRC/commit-page invalid -> invalid;
3. unsupported schema -> invalid unsupported;
4. length outside fixed buffer or slot capacity -> invalid length;
5. payload CRC mismatch -> invalid CRC;
6. deserialize and validate payload -> valid or invalid payload.

If exactly one slot is valid, use it. If both are valid, select the sequence that is newer using half-range unsigned comparison: `a` is newer than `b` when `a != b` and `uint32_t(a - b) < 0x80000000`. Exact equality is a duplicate-sequence tie; choose slot A deterministically and report a duplicate-sequence diagnostic. An exact half-range difference, where `uint32_t(a - b) == 0x80000000`, is ambiguous; choose slot A deterministically, do not claim either sequence is newer, and report `sequence_ambiguous`. In both duplicate and ambiguous A-selected cases, the next write target is slot B. Host tests cover equality, ordinary wrap from `0xFFFFFFFF` to `0`, and exact half-range ambiguity.

## Write-target and interrupted-write behavior

The selected valid slot is never erased while preparing its replacement. When one valid selected slot exists, Save targets the other slot. When both slots are valid, Save targets the slot that was not selected as newest; for duplicate-sequence or exact half-range ambiguity cases where slot A is selected deterministically, Save targets slot B. When neither slot is valid, the first attempted write targets slot A and uses initial sequence `0`. Otherwise, the next sequence is `(selected_sequence + 1) mod 2^32`. Factory reset uses the same target-selection and transaction rules; it does not erase both slots.

The old selected slot remains untouched until the target slot is fully erased, programmed, committed, read back, decoded, CRC-checked, and validated. Failure before final verification leaves the old selected slot authoritative. Interrupted erase of the target slot can destroy only that target slot; the previous committed slot remains valid. Interrupted programming before the commit page leaves the target uncommitted. Interrupted programming of the 256-byte commit page is decided only by exact commit-page validation, header CRC, payload CRC, and payload validation; it may produce either a valid committed slot or an invalid uncommitted slot. If both slots are invalid after repeated external interruption, factory defaults are used and the fallback reason reports both slot states.

## Schema-version policy

The record header contains `record_format_version` and `payload_schema_version`. Schema 1 payload does not duplicate the major schema version; it starts with two little-endian `uint16_t` control fields: `payload_schema_minor` at bytes 0..1 and `payload_flags_reserved` at bytes 2..3. Both must be zero for schema 1. Header and payload parsing tests follow this exact layout and reject unsupported `record_format_version`, unsupported `payload_schema_version`, nonzero control fields for schema 1, or payload lengths incompatible with schema 1. Unsupported records are never partially interpreted.

## Factory-default construction

Factory defaults are built from Feature 005 canonical constants, not from mutable runtime state. Channels 0..5 are the currently installed physical strips, mapped by board configuration to GP2..GP7, with pixel counts 132/174/141/81/96/72, physical length `0` mm, density `board::kDefaultPixelsPerMetre` currently 60, GRBW order, brightness 16, enabled true, reversed false, and six value-copied Gyver VU Gradient configurations with `enabled=true`, `source=stereo_left_right`, Off background, `vu_color_mode=level_position_gradient`, palette Green/Yellow/Orange/Red (`{0,255,0,0}`, `{255,255,0,0}`, `{255,128,0,0}`, `{255,0,0,0}`), `attack_ms=45`, and `release_ms=160`. Channels 6 and 7 are reserved for future hardware support and default exactly to `enabled=false`, `pixel_count=0`, Effect Off, GRBW order, brightness 16, `reversed=false`, `length_mm=0`, and `density_pixels_per_metre=0`; no GPIO is stored for any channel. Disabled Idle Lighting uses the Feature 004 default Idle values (`startup_idle_enabled=true`, `silence_timeout_ms=10000`, `audio_confirmation_ms=150`, idle colour `{0,0,0,255}`, `idle_brightness_q8=256`, fades 750/1500 ms, all currently supported input and channel masks, activity floors 32/32/32, hysteresis 4). Canonical global audio calibration defaults are `gyver_left_noise_floor=32`, `gyver_right_noise_floor=32`, `gyver_noise_gate_hysteresis=4`, `gyver_spectrum_noise_floor=256`, and `gyver_spectrum_minimum_peak=64`. Factory physical length remains zero unless an actual measured board or product constant is added later through an approved specification change. Temporary diagnostic scene cycling is disabled for normal Release startup.

## Boot sequence

Startup order becomes: initialize stdio as currently required; construct factory defaults; initialize flash backend bounds; inspect slots; choose newest valid supported record or factory defaults; validate selected configuration against current board capability; initialize LED output from board-supported enabled channel configs only while disabled channels adapt to runtime pixel count zero and no slice/transmission; initialize effect engine and Idle Lighting from board-supported runtime configs only; initialize audio capture; start normal loop. If LED initialization partially fails, audio still starts as in current behavior and diagnostics include usable board-supported channel count and configuration fallback status. If audio initialization fails, current fatal behavior is preserved.

## Controlled flash-save sequence

The save coordinator requests a save from non-real-time application code. Read-only slot inspection may occur before safe-point acquisition. It then acquires an LED-frame boundary using a compile-time bounded deadline and acquires an audio-safe boundary using a separate compile-time bounded deadline. If either deadline expires, Save aborts before activation and before issuing erase/program/commit for the write transaction, restores rendering/audio operation, leaves persisted storage untouched, leaves active and draft state as they were before the Save request, keeps dirty state unchanged, and records a typed `led_safe_point_timeout` or `audio_safe_point_timeout`. After both safe points are acquired, the coordinator prevents new optional rendering, snapshots dropped-block counters, pauses or deliberately masks capture/processing as required by the Pico SDK flash backend, performs any required structural activation, disables unsafe interrupts while executing flash erase/program functions from RAM when required, erases the target slot, programs header and payload data pages with the commit page still erased, verifies the uncommitted header and payload, programs the complete 256-byte commit page, verifies the committed record, restores interrupts/capture/rendering, and records duration plus deltas in diagnostics. Diagnostics distinguish LED safe-point wait time, audio safe-point wait time, and actual flash critical-section time. Operation-counter tests must prove no erase, program, or commit operation occurs before both safe points; harmless read-only flash inspection is allowed before safe points. The accepted interruption is bounded and observable; save is never initiated from audio ISR, effect render, frame packing, LED DMA callback, or LED transmission code.

## Flash layout and overlap checks

The storage layout shall be derived from the actual Pico W flash size and linker image, not from an assumed unsafe address. The Pico build shall reserve the persistent region at link/build time, or provide an equivalent build-time size assertion that fails before any flashable firmware artifact is produced when the application image, metadata, and persistent slots cannot coexist. Intentionally overlapping configurations are build/link-time negative tests only and must never be flashed to hardware. The build shall export linker symbols for application flash start/end and persistent region start/end, or an equivalent generated link-map value checked by firmware and verifier. Compile-time checks require each slot to be erase-sector aligned, slot size to be a multiple of both the RP2040 erase-sector size and 256-byte program-page size, the commit page to begin at a 256-byte-aligned address, and all programmed chunks to satisfy Pico SDK alignment. Runtime checks require persistent start/end to be within physical flash, sector-aligned, non-overlapping with the loaded image end, and non-overlapping any reserved bootloader/metadata areas. Host fake-flash tests simulate invalid runtime region descriptors and reject overlap before slot access.

## Diagnostics

Diagnostics are fixed-size summaries: boot source, fallback reason, slot A/B validity and sequence, selected sequence, dirty flag, last validation error, last save status, active-differs-from-persisted status, last save duration, interrupted audio blocks or dropped LED frames during save, factory-reset status, schema status, flash bounds, and generation. Release builds may expose bounded startup/telemetry summaries; development-only USB command helpers are disabled by default and are not a configuration protocol.

## Memory and flash budget

The design reserves two equal 4096-byte slots. Each slot has a 3840-byte data region containing the 64-byte header, 978-byte payload, and erased data padding, followed by the final 256-byte dedicated commit page at offset 3840. Static serialization buffers are fixed and sized by compile-time constants. No heap use is allowed in real-time paths; storage operations may use fixed stack or static buffers only. Firmware-size reporting must include the persistent region reservation and prove application image plus persistent slots fit in Pico W flash.

## Host-test strategy

Host tests exercise validation, factory defaults, serialization round trips, endian byte expectations, CRC32 check vector, record-format and payload-schema header parsing, schema-1 reserved flags, eight-channel schema records, reserved channels 6/7, current six-channel board capability, 300 accepted, 301 and 500 rejected on enabled current-board channels, total 800 accepted, total 801 rejected, disabled reserved channel retaining a schema-valid count without runtime publication, valid current Idle masks, reserved Idle bits rejected, eight-bit Idle mask codec round trips, rejection of unsupported enabled channels, absence of persisted GPIO by known-byte/exact-length tests, runtime adapters consuming only board-supported channels, total-pixel capability outside the schema, canonical Gyver calibration ownership, runtime overlay, serialization without duplicated calibration values, prevention of contradictory calibration state, known and unknown millimetre physical metadata, slot inspection, newest-valid selection, exact half-range ambiguity, duplicate sequence, deterministic write-target selection, selected-slot preservation, initial save to slot A, alternating slots, sequence wrap, fake-flash writes, write failure, readback failure, reset failure, corruption, truncation, length mismatch, malicious fields, unsupported schema, both slots invalid, interrupted erase, interrupted program before commit and at every relevant position in the 256-byte commit-page program, permanently busy LED output, continuously pending audio work, safe-point timeout recovery with proof that no erase/program/commit operation was issued, flash alignment, build-time negative overlap verification, and runtime fake-backend flash-overlap rejection. These tests live in the dedicated `device_configuration_tests` host suite. Verification runs all six suites—the five existing suites plus `device_configuration_tests`—under GCC, Clang, GCC ASan-only, and GCC UBSan-only through the authoritative verifier.

## Firmware integration strategy

Implementation proceeds behind stable adapters. First add pure host-testable modules, then fake storage, then Pico flash backend, then startup/load, then save/reset integration. Existing Features 001–004 behavior is preserved until the boot task intentionally replaces hard-coded factory startup with loaded-or-default configuration. Diagnostic scenes remain explicit bring-up tools and are not normal Release startup behavior.

## Failure handling

Invalid drafts are rejected without runtime side effects. Invalid persistent records fall back according to slot-selection rules. Flash backend failures preserve previous persisted state when possible. Reinitialization failures leave LEDs off or preserve a safe prior output state and report failure; audio fatal behavior remains unchanged. `panic()` is reserved for internal impossible states, not user-configurable invalid data.

## Trade-offs

- Binary slots are simpler and deterministic but require explicit schema maintenance.
- Two slots provide power-interruption resilience without filesystem complexity but store only one profile.
- Save may briefly interrupt real-time behavior because RP2040 executes from external flash; the interruption is accepted, bounded, and diagnosed.
- Structural LED changes are less immediate than effect/brightness previews, but this avoids unsafe buffer and DMA reconfiguration mid-frame.
- Feature 005 excludes networking and USB configuration to keep the storage layer stable before later UI work.
