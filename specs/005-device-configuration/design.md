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
- Existing runtime adapters in the LED, effects, Idle Lighting, audio, and startup layers consume only validated configurations through explicit integration functions.

Effects remain hardware-independent and never access storage or flash. LED output remains independent from audio, effects, web, Wi-Fi, and persistent storage internals. Storage never manipulates effect state directly; it stores only validated configuration values.

## Public interfaces

The implementation shall define fixed-capacity C++17 interfaces equivalent to:

```cpp
namespace config {
constexpr std::size_t kDeviceStripCount = 6;
constexpr uint16_t kMaximumPixelsPerStrip = board::kMaxPixelsPerStrip;
constexpr uint16_t kMaximumTotalPixels = board::kMaxConfiguredPixels;
constexpr uint32_t kCurrentSchemaVersion = 1;

struct PhysicalStripMetadata {
    // Informational only. Zero length means unknown or not measured.
    uint32_t length_micrometres;
    // Schema 1 stores 1..1000 pixels per metre; zero is invalid.
    uint16_t density_pixels_per_metre;
};

struct LedStripDeviceConfig {
    bool enabled;
    uint16_t pixel_count;
    uint32_t gpio;
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
// only and are excluded from persisted per-strip effect records. Idle Lighting
// activity floors and hysteresis remain owned by effects::IdleLightingConfig
// and are serialized in the Idle Lighting record: left_activity_floor,
// right_activity_floor, aux_activity_floor, and activity_hysteresis.

struct DeviceConfiguration {
    std::array<LedStripDeviceConfig, kDeviceStripCount> strips;
    std::array<effects::StripEffectConfig, kDeviceStripCount> effects;
    effects::IdleLightingConfig idle_lighting;
    AudioCalibrationConfig audio_calibration;
};
}
```

Result types shall be bounded enums with detail fields such as failing section, strip index, field id, slot id, and fallback reason. Diagnostics store only the latest bounded summaries and counters, not unbounded logs. Save coordination shall expose compile-time constants for LED-safe-point and audio-safe-point deadlines so timeout paths are deterministic and host-testable.

## Configuration state ownership

- Factory defaults: owned by `factory_defaults`; immutable value construction, no flash dependency.
- Active runtime configuration: owned by `ConfigService` and published by value to runtime adapters only at safe boundaries.
- Draft configuration: owned by `ConfigService`; edited through validated setters or whole-draft replacement.
- Last successfully persisted configuration: owned by `ConfigService`; updated only after a save or factory-reset write commits successfully, or after boot loads a valid supported record.

Dirty state is true when the draft differs from the last persisted configuration, or when active preview differs from last persisted after a successful preview but before Save. Equality is deterministic field-by-field comparison, not byte comparison of structs. Equality and dirty-state checks ignore duplicated Gyver calibration fields inside runtime `StripEffectConfig` mirrors and compare only `DeviceConfiguration::audio_calibration` for those values.

## Validation flow

Field preview validation runs first and rejects invalid enum values, incompatible effect/source pairs, out-of-range effect parameters, brightness outside 0..255, Idle bounds, and safe audio-calibration bounds. Whole-configuration validation then checks six-strip invariants, fixed GP2–GP7 mapping, duplicate/invalid mappings, enabled-strip pixel counts, stored disabled-strip pixel counts, total enabled pixel count, physical metadata ranges, serialization-size limits, and all integer overflow risks. A disabled strip may store `pixel_count == 0` or retain a valid nonzero pixel count up to the per-strip maximum; disabled strips do not contribute to the total enabled-pixel limit, are rendered LED-off, and are not transmitted until re-enabled through a structural activation boundary. `pixel_count` is the sole operational rendering/transmission authority. `length_micrometres` and `density_pixels_per_metre` are informational only; changing either never recalculates or mutates `pixel_count`. `length_micrometres == 0` means unknown or not measured and is valid. Schema 1 accepts density 1..1000 pixels per metre and rejects zero density. Approximate or unknown physical metadata must not make validation reject an otherwise valid authoritative pixel count.

Persistent validation additionally checks schema version, payload length, CRC32, commit marker, slot alignment, erase/program alignment, slot bounds, and application-image overlap. Future schema versions are classified as unsupported and are never interpreted as current payloads.

## Preview and activation flow

Previewable non-structural changes are effects, colours, visual parameters, strip brightness, Idle Lighting settings, and safe audio calibration. Effect and Idle changes may be atomically published at the existing render boundary: the service validates the draft subset and stages effect and Idle configs through their existing staged APIs. Before staging any applicable Gyver runtime effect configuration, a configuration adapter overlays `DeviceConfiguration::audio_calibration` into the runtime mirror fields currently present in `effects::StripEffectConfig`. Reading active or draft configuration normalizes those mirrored fields from the canonical global calibration, so contradictory per-strip calibration state cannot be observed through Feature 005 public APIs or serialized. Audio calibration is staged by value in fixed-capacity storage and consumed by audio processing only at an audio-block boundary or another named audio-safe point; it performs no heap allocation, flash access, or blocking diagnostics in the audio path. If any affected layer rejects the staged value, the whole publication fails and the active configuration is unchanged. The configuration generation increments only after all affected layers accept the change.

Structural LED changes are enabled state, pixel count, channel order, reversal, and equivalent output-layout changes. They are accepted into the draft only after validation but are not applied mid-frame. Activation is a two-phase operation: first validate and precompute every required fixed-capacity slice, span, and hardware-resource plan without touching live output; then switch at a controlled LED-output reinitialization boundary or controlled restart. At that boundary, rendering is paused, any in-flight LED frame reaches idle/latch completion or times out safely, the `LedOutputManager` is reconfigured, effect spans are rebuilt, and rendering resumes. If hardware reinitialization partially fails and rollback is not possible, LEDs enter a safe-off or prior-safe-output state, active configuration remains reported as the previous configuration, dirty remains true, and diagnostics identify the failed phase and strip/resource.

Publication is all-or-nothing. If any layer rejects a validated publication, no new active configuration is reported; diagnostics record the layer and reason, and the draft remains available for correction or discard.

## Save, reload, and reset semantics

Save validates the whole draft, activates it if needed at the correct boundary, serializes it into a fixed buffer, writes the next slot, verifies the committed record, updates last-persisted state, and clears dirty state. Failure semantics are phase-specific: validation failure leaves draft intact and active unchanged; activation failure leaves active and last-persisted unchanged; flash write or verification failure after successful activation leaves the previewed active configuration running, leaves last-persisted unchanged, keeps dirty true, and reports that active differs from persisted. Reload/discard is the explicit rollback path after such a failed save.

Reload/discard replaces draft with last-persisted configuration, or factory defaults if no persisted configuration exists. If preview had changed active non-structural values, reload republishes the restored values at a safe boundary. Structural rollback follows the same controlled reinitialization/restart rule.

Factory reset requires a confirmation token or equivalent explicit two-step API that cannot be triggered by malformed draft data. Reset constructs factory defaults, activates them safely, writes them as a new committed record, updates last-persisted state after verification, and clears dirty state. If the reset write fails, factory defaults may remain active but diagnostics must distinguish active factory fallback from persisted reset success.

## Binary payload format

All multibyte fields are little-endian unsigned integers. Boolean fields are one byte `0` or `1`; any other value is invalid. Enums are stored as stable explicit numeric ids documented by the codec, not by compiler object layout. Reserved bytes are written as zero and must be zero for schema 1 unless explicitly documented otherwise.

Payload schema 1 order:

1. payload schema minor flags, currently zero;
2. six LED strip records;
3. six effect records using stable ids for every persisted Feature 004 parameter except duplicated Gyver calibration mirror fields;
4. one Idle Lighting record, including Idle Lighting activity floors and hysteresis;
5. one global audio calibration record containing the only serialized Gyver VU/spectrum calibration values;
6. payload CRC input covers exactly these bytes.

The codec is deterministic and host-testable: serializing the same value twice produces identical bytes, and deserializing then serializing a valid payload reproduces the canonical byte sequence. Codec round trips, schema tests, and factory-default tests use only the global audio-calibration fields for Gyver calibration. Any noncanonical duplicated calibration values in input effect mirrors are normalized from the global calibration before comparison and are never emitted into the payload.

## Record and slot format

Each fixed slot begins on a flash erase-sector boundary and has equal size. A record contains:

- header magic, for example `PMLC`;
- header format version;
- payload schema version;
- payload length;
- monotonic 32-bit sequence;
- payload CRC32;
- header CRC32 excluding mutable commit bytes;
- commit marker stored in a separate programmed word;
- payload bytes;
- optional erased padding to the slot size.

The erased state is all `0xFF`. The commit word's erased and uncommitted state is `0xFFFFFFFF`; the committed state is an exact non-erased constant, for example `0x434F4D30` (`COM0`), whose transition from erased changes only `1` bits to `0` bits. Any partial marker value is invalid. Tests must reject illegal marker transitions and partially programmed marker words. A new slot is erased, programmed with header/payload while the commit word remains uncommitted, verified by readback, and only then programmed with the committed marker. The old slot is not erased as part of making the new slot authoritative.

## Slot-selection algorithm

At boot and after writes, inspect both slots independently:

1. erased or missing slot -> invalid with erased/missing reason;
2. magic/header/header CRC/commit invalid -> invalid;
3. unsupported schema -> invalid unsupported;
4. length outside fixed buffer or slot capacity -> invalid length;
5. payload CRC mismatch -> invalid CRC;
6. deserialize and validate payload -> valid or invalid payload.

If exactly one slot is valid, use it. If both are valid, select the sequence that is newer using half-range unsigned comparison: `a` is newer than `b` when `a != b` and `uint32_t(a - b) < 0x80000000`. Exact equality is a duplicate-sequence tie; choose slot A deterministically and report a duplicate-sequence diagnostic. An exact half-range difference, where `uint32_t(a - b) == 0x80000000`, is ambiguous; choose slot A deterministically, do not claim either sequence is newer, and report `sequence_ambiguous`. In both duplicate and ambiguous A-selected cases, the next write target is slot B. Host tests cover equality, ordinary wrap from `0xFFFFFFFF` to `0`, and exact half-range ambiguity.

## Write-target and interrupted-write behavior

The selected valid slot is never erased while preparing its replacement. When one valid selected slot exists, Save targets the other slot. When both slots are valid, Save targets the slot that was not selected as newest; for duplicate-sequence or exact half-range ambiguity cases where slot A is selected deterministically, Save targets slot B. When neither slot is valid, the first attempted write targets slot A and uses initial sequence `0`. Otherwise, the next sequence is `(selected_sequence + 1) mod 2^32`. Factory reset uses the same target-selection and transaction rules; it does not erase both slots.

The old selected slot remains untouched until the target slot is fully erased, programmed, committed, read back, decoded, CRC-checked, and validated. Failure before final verification leaves the old selected slot authoritative. Interrupted erase of the target slot can destroy only that target slot; the previous committed slot remains valid. Interrupted programming before the commit marker leaves the target uncommitted. Interrupted programming of the commit marker is detected by exact marker validation, header CRC, payload CRC, and payload validation. If both slots are invalid after repeated external interruption, factory defaults are used and the fallback reason reports both slot states.

## Schema-version policy

Schema major version is stored in the record and payload. Feature 005 supports schema 1 only. Future versions greater than the compiled supported version are unsupported. Older versions are unsupported until a migration task is specified; Feature 005 does not implement migration. Unsupported records are never partially interpreted.

## Factory-default construction

Factory defaults are built from Feature 005 canonical constants, not from mutable runtime state: `board::kStripGpios`; pixel counts 132/174/141/81/96/72; physical length `0` micrometres for every strip; density `board::kDefaultPixelsPerMetre`; GRBW order; brightness 16; enabled true; reversed false; six value-copied Gyver VU Gradient configurations with `enabled=true`, `source=stereo_left_right`, Off background, `vu_color_mode=level_position_gradient`, palette Green/Yellow/Orange/Red (`{0,255,0,0}`, `{255,255,0,0}`, `{255,128,0,0}`, `{255,0,0,0}`), `attack_ms=45`, and `release_ms=160`; disabled Idle Lighting with the Feature 004 default Idle values (`startup_idle_enabled=true`, `silence_timeout_ms=10000`, `audio_confirmation_ms=150`, idle colour `{0,0,0,255}`, `idle_brightness_q8=256`, fades 750/1500 ms, all input and strip masks, activity floors 32/32/32, hysteresis 4); and canonical global audio calibration defaults `gyver_left_noise_floor=32`, `gyver_right_noise_floor=32`, `gyver_noise_gate_hysteresis=4`, `gyver_spectrum_noise_floor=256`, and `gyver_spectrum_minimum_peak=64`. Factory physical length remains zero unless an actual measured board or product constant is added later through an approved specification change. Temporary diagnostic scene cycling is disabled for normal Release startup.

## Boot sequence

Startup order becomes: initialize stdio as currently required; construct factory defaults; initialize flash backend bounds; inspect slots; choose newest valid supported record or factory defaults; validate selected configuration; initialize LED output from selected LED config; initialize effect engine and Idle Lighting from selected runtime config; initialize audio capture; start normal loop. If LED initialization partially fails, audio still starts as in current behavior and diagnostics include usable strip count and configuration fallback status. If audio initialization fails, current fatal behavior is preserved.

## Controlled flash-save sequence

The save coordinator requests a save from non-real-time application code. It acquires an LED-frame boundary using a compile-time bounded deadline and acquires an audio-safe boundary using a separate compile-time bounded deadline. If either deadline expires, Save aborts before issuing any flash read/erase/program operation for the write transaction, restores rendering/audio operation, leaves persisted storage untouched, leaves active and draft state as they were before the Save request, keeps dirty state unchanged, and records a typed `led_safe_point_timeout` or `audio_safe_point_timeout`. After both safe points are acquired, the coordinator prevents new optional rendering, snapshots dropped-block counters, pauses or deliberately masks capture/processing as required by the Pico SDK flash backend, disables unsafe interrupts while executing flash erase/program functions from RAM when required, writes and verifies the target slot, restores interrupts/capture/rendering, and records duration plus deltas in diagnostics. Diagnostics distinguish LED safe-point wait time, audio safe-point wait time, and actual flash critical-section time. The accepted interruption is bounded and observable; save is never initiated from audio ISR, effect render, frame packing, LED DMA callback, or LED transmission code.

## Flash layout and overlap checks

The storage layout shall be derived from the actual Pico W flash size and linker image, not from an assumed unsafe address. The Pico build shall reserve the persistent region at link/build time, or provide an equivalent build-time size assertion that fails before any flashable firmware artifact is produced when the application image, metadata, and persistent slots cannot coexist. Intentionally overlapping configurations are build/link-time negative tests only and must never be flashed to hardware. The build shall export linker symbols for application flash start/end and persistent region start/end, or an equivalent generated link-map value checked by firmware and verifier. Compile-time checks require slot size to be a multiple of the RP2040 erase size and programmed chunks to satisfy SDK alignment. Runtime checks require persistent start/end to be within physical flash, sector-aligned, non-overlapping with the loaded image end, and non-overlapping any reserved bootloader/metadata areas. Host fake-flash tests simulate invalid runtime region descriptors and reject overlap before slot access.

## Diagnostics

Diagnostics are fixed-size summaries: boot source, fallback reason, slot A/B validity and sequence, selected sequence, dirty flag, last validation error, last save status, active-differs-from-persisted status, last save duration, interrupted audio blocks or dropped LED frames during save, factory-reset status, schema status, flash bounds, and generation. Release builds may expose bounded startup/telemetry summaries; development-only USB command helpers are disabled by default and are not a configuration protocol.

## Memory and flash budget

The design reserves two equal slots sized for the maximum schema-1 payload plus header, CRC, commit marker, and padding. Initial target is two 4 KiB erase sectors or two larger erase-aligned slots if the encoded effect payload requires it. Static serialization buffers are fixed and sized by compile-time constants. No heap use is allowed in real-time paths; storage operations may use fixed stack or static buffers only. Firmware-size reporting must include the persistent region reservation and prove application image plus persistent slots fit in Pico W flash.

## Host-test strategy

Host tests exercise validation, factory defaults, serialization round trips, endian byte expectations, CRC32 known vectors, canonical Gyver calibration ownership, runtime overlay, serialization without duplicated calibration values, prevention of contradictory calibration state, known and unknown physical metadata, slot inspection, newest-valid selection, exact half-range ambiguity, duplicate sequence, deterministic write-target selection, selected-slot preservation, initial save to slot A, alternating slots, sequence wrap, fake-flash writes, write failure, readback failure, reset failure, corruption, truncation, length mismatch, malicious fields, unsupported schema, both slots invalid, interrupted erase, interrupted program before and during commit, permanently busy LED output, continuously pending audio work, safe-point timeout recovery with proof that no flash operation was issued, flash alignment, build-time negative overlap verification, and runtime fake-backend flash-overlap rejection. Tests run in the existing five-suite host architecture or in a sixth focused suite only after the task adds it to every GCC/Clang/sanitizer configuration.

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
