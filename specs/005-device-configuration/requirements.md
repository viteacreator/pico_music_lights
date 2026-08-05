# Feature 005 — Device Configuration and Persistent Storage Requirements

## Goal

Feature 005 shall provide one validated device-configuration model and one reliable persistent-storage layer for later features. Later Wi-Fi and web UI features shall consume these interfaces, but Feature 005 shall not implement Wi-Fi, access point mode, HTTP, JSON, a web interface, OTA, additional effects, or a USB configuration protocol.

## Mandatory requirements

### Scope and lifecycle

R005-001. The firmware shall expose exactly one persistent device profile.

R005-002. Multiple user presets shall not be implemented in Feature 005.

R005-003. The configuration subsystem shall represent four separated states: factory defaults, currently active runtime configuration, draft or pending configuration, and last successfully persisted configuration.

R005-004. Factory defaults shall be constructible without flash access and shall be used when no supported valid persistent record can be loaded.

R005-005. The currently active runtime configuration shall be the only configuration consumed by rendering, LED output, Idle Lighting, and audio calibration during normal operation.

R005-006. Draft changes shall be validated before preview and shall not overwrite the last successfully persisted configuration until an explicit Save succeeds.

R005-007. The subsystem shall track whether the draft or active preview differs from the last successfully persisted configuration.

R005-008. Reload or discard shall restore the draft and previewable active values from the last successfully persisted configuration, or from factory defaults when no valid record has ever been loaded or saved.

R005-009. Factory reset shall require an explicit confirmation mechanism and shall restore factory defaults safely.

R005-010. Flash writing shall be initiated only by explicit Save or confirmed factory reset.

### Persisted content

R005-011. The persisted profile shall include the complete LED-channel configuration for all eight persistent LED channels.

R005-012. Each LED-channel configuration shall include enabled state, pixel count, channel order, reversal, brightness, and physical metadata for strip length and LED density. It shall not include GPIO. Pixel count is always the sole operational authority for rendering and transmission; physical length and density are informational metadata only.

R005-013. GPIO assignment shall remain fixed by board configuration, mapped from stable logical channel index, and shall not be user-configurable or serialized. Dirty-state comparison shall not compare GPIO, loading shall not read GPIO from flash, and future GPIO assignments for channels 6 and 7 shall be introduced only in board configuration after the hardware is defined.

R005-014. Pixel count shall be the authoritative operational value used by rendering and output even when physical length and density metadata are stored. Changing physical length or density shall never implicitly change `pixel_count`.

R005-015. The persisted profile shall include eight effect records, one per persistent logical LED channel. Current runtime adapters shall publish only board-supported channels to the current six-channel LED driver and EffectEngine; the specification shall not claim that current runtime code already supports eight active channels.

R005-016. The persisted profile shall include the global Idle Lighting configuration.

R005-017. Idle Lighting shall remain disabled in factory defaults.

R005-018. The persisted profile shall include canonical device-level Gyver audio calibration values currently required for correct runtime behavior: Gyver VU left and right noise floors, Gyver VU hysteresis, Gyver spectrum noise floor, and Gyver spectrum minimum peak. These values shall be owned only by the global audio-calibration record, serialized exactly once, excluded from per-channel persisted effect records, overlaid into applicable Gyver runtime effect configurations during publication, and used as the only source for dirty-state comparison, equality, factory defaults, codec round trips, and schema tests. Conflicting duplicated values from runtime `effects::StripEffectConfig` instances shall be normalized from the global calibration and shall not enter the persisted payload. Idle Lighting activity floors and hysteresis shall remain owned by `effects::IdleLightingConfig` and shall not be duplicated in `AudioCalibrationConfig`.

R005-019. Wi-Fi credentials and all networking configuration shall be excluded and deferred to Feature 006.

### Factory defaults

R005-020. The persistent configuration shall define `kMaximumLedChannelCount = 8`; logical channel index shall be the stable identity. The current board supports channels 0 through 5, mapped by board configuration to GP2, GP3, GP4, GP5, GP6, and GP7 with factory pixel counts 132, 174, 141, 81, 96, and 72 respectively. Channels 6 and 7 are reserved for future hardware support and have no stored GPIO.

R005-021. Factory LED channel order shall be GRBW for current channels 0 through 5; reserved channels 6 and 7 shall store a harmless supported default channel order while disabled.

R005-022. Factory LED reversal shall be false for all eight persistent channels.

R005-023. Factory LED brightness shall be 16 out of 255 for all eight persistent channels unless a later approved specification chooses another harmless stored default for disabled reserved channels.

R005-024. Factory physical density shall be `board::kDefaultPixelsPerMetre`, currently 60 pixels per metre, for currently installed channels 0 through 5. Factory physical length shall be `0` millimetres for all channels, meaning unknown or not measured, unless an actual measured board or product constant is added later through an approved specification change. Reserved channels 6 and 7 shall store unknown length and unknown density. Persistent loading overrides factory defaults only after a valid record has been selected.

R005-025. The current six independent Gyver VU effect configurations shall remain the factory effect defaults for channels 0 through 5. Reserved channels 6 and 7 shall default to disabled, zero pixels, Effect Off, brightness 16, not reversed, unknown physical length, and unknown density.

R005-026. Factory defaults shall not auto-run temporary diagnostic scenes during normal Release startup.

R005-027. Diagnostic scenes may remain explicitly accessible for hardware bring-up only.

### Runtime editing and activation

R005-028. Runtime editing shall use live preview followed by explicit Save.

R005-029. Effect parameters, colours, brightness, Idle Lighting settings, and safe audio-calibration values may be previewed live after field-level validation.

R005-030. Structural LED changes, including enabled state, pixel count, channel order, reversal, and equivalent output-layout changes, shall be applied only through controlled LED-output reinitialization or a controlled restart boundary.

R005-031. Whole-configuration publication shall be atomic at safe runtime boundaries.

R005-032. No dynamic allocation shall occur in real-time audio acquisition, audio processing, effect rendering, frame preparation, or LED transmission paths.

R005-033. Configuration shared across runtime layers shall be transferred by bounded fixed-capacity data structures.

### Validation

R005-034. Validation shall require every logical channel index to be within the fixed eight-channel persistent capacity. It shall reject enabling a channel that is not supported by the current board configuration. Future board support may increase the supported channel count from six to eight without changing the Feature 005 persistent schema or requiring migration solely for the planned channels.

R005-035. Validation shall reject per-channel pixel counts outside the `uint16_t` representation, including zero for an enabled board-supported channel. Disabled channels may store zero or a retained valid nonzero pixel count.

R005-036. Validation shall reject total enabled board-supported pixels above the current board/runtime total-pixel capability. Total calculations shall use at least `uint32_t`. The persistent codec can represent eight channels at each channel's `uint16_t` maximum; board total-pixel capability is not part of the persistent schema, and changing that capability shall not require changing the persistent record format. Disabled and unsupported channels shall not contribute to the total and shall produce LED-off/no-transmission behavior.

R005-037. Validation shall reject any persisted or draft attempt to provide hardware mappings, because GPIO is not a persisted field. Runtime adapters shall map each enabled board-supported logical channel to board configuration and shall consume only board-supported channels.

R005-038. Validation shall reject brightness values outside 0..255 and unsupported channel-order enumeration values.

R005-039. Validation shall reject unsupported effect identifiers, incompatible effect sources, and effect parameters outside the Feature 004 bounds.

R005-040. Validation shall reject Idle Lighting values outside Feature 004 Idle bounds.

R005-041. Validation shall reject unsafe audio calibration and noise-floor values outside explicitly documented ranges. Validation shall allow `length_mm == 0` as unknown physical length, shall use `uint16_t length_mm` with maximum representable length 65,535 mm, shall not use micrometre precision, and shall treat millimetre precision as sufficient. Validation shall accept `density_pixels_per_metre == 0` as unknown and nonzero density values in the explicit implementation range 1..1000 pixels per metre. Arithmetic consistency checks shall not reject a valid authoritative pixel count merely because optional physical metadata is unknown or approximate.

R005-042. Validation shall check serialized lengths and all integer arithmetic for overflow before allocating fixed buffers, copying bytes, or accepting a record.

R005-043. Validation shall reject unsupported schema versions and shall not interpret future schemas as the current schema.

R005-044. Validation shall check persistent slot placement, flash erase/program alignment, and persistent-region size.

R005-045. Firmware shall provide compile-time and runtime checks that the persistent region does not overlap the application image or any reserved flash area. An intentionally overlapping configuration shall be a build/link-time negative test that fails before any flashable firmware artifact is produced and shall never be flashed to hardware.

### Persistent format and slots

R005-046. Persistent storage shall use an explicitly serialized binary format.

R005-047. The implementation shall not persist raw C++ structure memory with `memcpy` or equivalent object-representation copying.

R005-048. JSON, LittleFS, and other filesystems shall not be used for Feature 005 storage.

R005-049. Storage shall use two fixed persistent slots, A and B.

R005-050. Each slot record shall include at minimum a magic value, schema version, payload length, monotonic sequence or generation value, payload CRC32, and commit-validity mechanism.

R005-051. Serialization and deserialization shall be deterministic, endian-defined, bounds-checked, and independently host-testable.

R005-052. A newly written slot shall not become authoritative until its complete payload and integrity metadata are valid.

R005-053. Interrupted erase or programming shall leave either the previous valid configuration recoverable or safe factory defaults available.

R005-054. Loading shall select the newest valid supported record.

R005-055. If one slot is invalid and the other is valid, loading shall use the valid slot.

R005-056. If both slots are invalid, missing, erased, unsupported, or corrupt, loading shall use factory defaults and expose the fallback reason through bounded diagnostics.

R005-057. Sequence comparison shall define wrap behavior and shall be host-tested at wrap boundaries. For two valid slots, the ordinary newer comparison shall use half-range unsigned arithmetic; exact sequence equality shall select slot A deterministically and report duplicate sequence; an exact `0x80000000` difference shall be ambiguous, shall select slot A deterministically without claiming either sequence is newer, shall report `sequence_ambiguous`, and shall target slot B for the next write.

R005-058. Save and factory reset shall use deterministic write-target selection that never erases the selected valid slot while preparing its replacement. When one valid selected slot exists, the target shall be the other slot; when both slots are valid, the target shall be the non-selected slot; when neither slot is valid, the first attempted write shall target slot A with initial sequence `0`; otherwise the next sequence shall be `(selected_sequence + 1) mod 2^32`. The old selected slot shall remain authoritative until the target slot has been fully programmed, committed, read back, decoded, CRC-checked, and validated. Failure before final verification shall leave the old selected slot authoritative. Factory reset shall use the same transaction and shall not erase both slots.

### Boot, save, and diagnostics

R005-059. Normal Release startup shall load the newest valid supported configuration or factory defaults before enabling normal rendering.

R005-060. The controlled save sequence shall reach an LED-frame boundary before erase/program operations begin, using a finite compile-time bounded deadline. If the LED-safe boundary cannot be acquired before that deadline, Save shall abort before any flash erase/program operation, restore runtime operation, leave active/draft/persisted states unchanged except bounded diagnostics, and report a typed LED-safe-point timeout.

R005-061. The controlled save sequence shall prevent unsafe concurrent execution from flash during flash erase/program operations.

R005-062. The controlled save sequence shall handle audio capture and processing deliberately, either by pausing and accounting for dropped work or by proving that the selected flash backend is safe for the active capture mode. Audio-safe boundary acquisition shall use a finite compile-time bounded deadline; timeout shall abort before any flash erase/program operation, restore runtime operation, leave active/draft/persisted states unchanged except bounded diagnostics, and report a typed audio-safe-point timeout.

R005-063. Runtime operation shall be restored after save or reset whether the storage operation succeeds or fails, unless a pre-existing unrecoverable hardware fault prevents restoration.

R005-064. A short, bounded real-time interruption during flash erase/programming is accepted on RP2040.

R005-065. Save duration and relevant dropped-frame or interrupted-processing information shall be observable through bounded diagnostics. The measured duration shall distinguish LED/audio safe-point wait time from actual flash critical-section time.

R005-066. Fallback reasons, slot validity summaries, last save status, whether active differs from persisted, last reset status, schema status, and flash-overlap failures shall be observable through bounded diagnostics.

### Verification

R005-067. Host tests shall cover positive, boundary, corruption, truncation, sequence-wrap, exact half-range ambiguity, duplicate sequence, unsupported-schema, both-slots-invalid, interrupted-erase, interrupted-program, CRC mismatch, length mismatch, malicious-field, eight-channel schema records, current six-channel board capability, reserved channels 6 and 7 disabled, rejection of unsupported enabled channels, absence of persisted GPIO, current runtime adapters consuming only board-supported channels, total-pixel capability outside the schema, canonical Gyver calibration ownership, runtime calibration overlay, serialization without duplicated calibration values, prevention of contradictory calibration state, known/unknown millimetre physical metadata, deterministic write-target selection, preservation of the selected slot, initial save, alternating slots, write failure, readback failure, reset failure, bounded LED/audio safe-point timeout recovery, proof that no flash operation occurs before safe points, and flash-overlap cases.

R005-068. Feature 005 implementation tasks shall leave the repository compilable after each milestone.

R005-069. Final software verification shall run the authoritative clean verifier command from `AGENTS.md`.

## Non-goals

N005-001. Feature 005 shall not implement Wi-Fi, access point mode, HTTP, JSON, a web interface, OTA, or networking configuration.

N005-002. Feature 005 shall not implement a USB configuration protocol.

N005-003. Development-only USB diagnostics may be retained or added only when strictly necessary for debugging and shall be disabled by default in Release builds.

N005-004. Feature 005 shall not implement additional effects.

N005-005. Feature 005 shall not reopen Features 001–004 for optional refactoring or improvement.
