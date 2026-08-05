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

R005-011. The persisted profile shall include the complete LED configuration for all six strips.

R005-012. Each strip LED configuration shall include enabled state, pixel count, channel order, reversal, brightness, fixed GPIO identity, and physical metadata for strip length and LED density when these values exist in the model.

R005-013. GPIO assignment shall remain fixed by the board configuration and shall not be user-configurable.

R005-014. Pixel count shall be the authoritative operational value used by rendering and output even when physical length and density metadata are stored.

R005-015. The persisted profile shall include each strip's effect configuration.

R005-016. The persisted profile shall include the global Idle Lighting configuration.

R005-017. Idle Lighting shall remain disabled in factory defaults.

R005-018. The persisted profile shall include audio calibration values and noise-floor values currently required for correct runtime behavior, including Gyver VU left and right noise floors, Gyver VU hysteresis, Gyver spectrum noise floor, Gyver spectrum minimum peak, and Idle Lighting activity floors and hysteresis.

R005-019. Wi-Fi credentials and all networking configuration shall be excluded and deferred to Feature 006.

### Factory defaults

R005-020. The factory LED layout shall be six enabled strips on GP2, GP3, GP4, GP5, GP6, and GP7 with pixel counts 132, 174, 141, 81, 96, and 72 respectively.

R005-021. Factory LED channel order shall be GRBW for every strip.

R005-022. Factory LED reversal shall be false for every strip.

R005-023. Factory LED brightness shall be 16 out of 255 for every strip.

R005-024. Factory physical LED density shall default to 60 pixels per metre for every strip unless a more specific stored value is valid.

R005-025. The current six independent Gyver VU effect configurations shall remain the factory effect defaults.

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

R005-034. Validation shall reject any configuration whose strip count differs from six.

R005-035. Validation shall reject per-strip pixel counts outside the configured limits, including zero for an enabled strip and counts above the board maximum.

R005-036. Validation shall reject total enabled pixels above the configured total-pixel limit. Disabled strips may store zero pixels or a retained valid nonzero pixel count, but disabled-strip pixels shall not contribute to the total enabled-pixel limit and disabled strips shall produce LED-off/no-transmission behavior until re-enabled through a structural activation boundary.

R005-037. Validation shall reject invalid or duplicate hardware mappings and any GPIO mapping that differs from the fixed board mapping.

R005-038. Validation shall reject brightness values outside 0..255 and unsupported channel-order enumeration values.

R005-039. Validation shall reject unsupported effect identifiers, incompatible effect sources, and effect parameters outside the Feature 004 bounds.

R005-040. Validation shall reject Idle Lighting values outside Feature 004 Idle bounds.

R005-041. Validation shall reject unsafe audio calibration and noise-floor values outside explicitly documented ranges.

R005-042. Validation shall check serialized lengths and all integer arithmetic for overflow before allocating fixed buffers, copying bytes, or accepting a record.

R005-043. Validation shall reject unsupported schema versions and shall not interpret future schemas as the current schema.

R005-044. Validation shall check persistent slot placement, flash erase/program alignment, and persistent-region size.

R005-045. Firmware shall provide compile-time and runtime checks that the persistent region does not overlap the application image or any reserved flash area.

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

R005-057. Sequence comparison shall define wrap behavior and shall be host-tested at wrap boundaries.

### Boot, save, and diagnostics

R005-058. Normal Release startup shall load the newest valid supported configuration or factory defaults before enabling normal rendering.

R005-059. The controlled save sequence shall reach an LED-frame boundary before erase/program operations begin.

R005-060. The controlled save sequence shall prevent unsafe concurrent execution from flash during flash erase/program operations.

R005-061. The controlled save sequence shall handle audio capture and processing deliberately, either by pausing and accounting for dropped work or by proving that the selected flash backend is safe for the active capture mode.

R005-062. Runtime operation shall be restored after save or reset whether the storage operation succeeds or fails, unless a pre-existing unrecoverable hardware fault prevents restoration.

R005-063. A short, bounded real-time interruption during flash erase/programming is accepted on RP2040.

R005-064. Save duration and relevant dropped-frame or interrupted-processing information shall be observable through bounded diagnostics.

R005-065. Fallback reasons, slot validity summaries, last save status, whether active differs from persisted, last reset status, schema status, and flash-overlap failures shall be observable through bounded diagnostics.

### Verification

R005-066. Host tests shall cover positive, boundary, corruption, truncation, sequence-wrap, unsupported-schema, both-slots-invalid, interrupted-erase, interrupted-program, CRC mismatch, length mismatch, malicious-field, and flash-overlap cases.

R005-067. Feature 005 implementation tasks shall leave the repository compilable after each milestone.

R005-068. Final software verification shall run the authoritative clean verifier command from `AGENTS.md`.

## Non-goals

N005-001. Feature 005 shall not implement Wi-Fi, access point mode, HTTP, JSON, a web interface, OTA, or networking configuration.

N005-002. Feature 005 shall not implement a USB configuration protocol.

N005-003. Development-only USB diagnostics may be retained or added only when strictly necessary for debugging and shall be disabled by default in Release builds.

N005-004. Feature 005 shall not implement additional effects.

N005-005. Feature 005 shall not reopen Features 001–004 for optional refactoring or improvement.
