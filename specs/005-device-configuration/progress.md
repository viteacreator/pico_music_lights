# Feature 005 — Progress

Status: software implementation and verification complete; owner physical
validation remains pending.

PR #3 must remain draft until the completion gate passes. Physical validation has not been performed and remains owner-only.

## Requirement-to-implementation matrix

Every row distinguishes implementation, runtime integration, host evidence, Pico evidence, remaining work, and exact files. “Complete” below means the software task is implemented and has focused evidence; it does not claim physical hardware validation.

| Task | Implementation status | Runtime integration status | Host-test coverage | Pico build coverage | Remaining work | Exact source and test files |
|---:|---|---|---|---|---|---|
| 1 | Complete: fixed eight-channel schema, typed limits/results, no GPIO. | Current board remains six-channel. | Defaults, no-GPIO trait, memory limits. | Compiled. | Final gate only. | `include/config/device_config.hpp`, `tests/host/device_configuration_tests.cpp` |
| 2 | Complete: one global calibration owner and overlay. | Published to all applicable runtime effects. | `canonical_calibration_overlay`. | Compiled/published at boot. | Final gate only. | `src/config/device_config.cpp`, `src/config/runtime_adapter.cpp` |
| 3 | Complete: immutable eight-channel factory construction. | Used for empty boot and reset. | `factory_defaults_all_eight_channels`. | Boot integration compiled. | Physical observation only. | `src/config/device_config.cpp`, `src/config/config_boot.cpp` |
| 4 | Complete: channel, total, effect/source, Idle, calibration and canonical field validation with section/field/reason. | Enforced before adaptation/publication. | `validation_boundaries`, `validation_named_boundaries`. | Compiled. | Final gate only. | `src/config/device_config.cpp`, `include/config/device_config.hpp` |
| 5 | Complete: factory/active/draft/last-verified/presence and dirty are distinct. | Coordinator advances active and persisted states at their respective boundaries. | transition, failure, reload tests. | Constructed in firmware. | Final gate only. | `src/config/config_service.cpp`, `src/storage/save_coordinator.cpp` |
| 6 | Complete: exact explicit codec, schema-ID maps and per-effect canonicalization. | Used for boot/Save. | round-trip, golden, malformed, all-effect canonicalization, extra-length/no-GPIO. | Compiled. | Final gate only. | `src/config/device_config_codec.cpp`, `src/config/schema_enum_mapping.cpp` |
| 7 | Complete: CRC-32/ISO-HDLC. | Header/payload store integrity. | check vector and record mutation coverage. | Compiled. | Final gate only. | `src/config/crc32.cpp`, record tests |
| 8 | Complete: exact 64/978/3840/4096/256 layout and inspection. | Used at boot and post-commit. | golden header/records, reserved, CRC, padding and commit tests. | Compiled. | Final gate only. | `src/storage/persistent_record.cpp` |
| 9 | Complete: wrap, duplicate and exact half-range selection. | Used by boot/store. | named selection boundary tests. | Compiled. | Final gate only. | `src/storage/persistent_record.cpp` |
| 10 | Complete: deterministic target/sequence without selected-slot erase. | Used by every Save/reset. | first/alternate/wrap/preservation tests. | Compiled. | Final gate only. | `src/storage/persistent_record.cpp`, `src/storage/device_config_store.cpp` |
| 11 | Complete: precommit authority and postcommit valid/invalid/unknown model. | Coordinator preserves in-RAM baseline on failure/unknown. | exhaustive commit, previous-slot and reboot outcomes. | Compiled. | Physical representative interruption only. | `src/storage/device_config_store.cpp`, coordinator tests |
| 12 | Complete: backend abstraction, geometry and counters. | RP and fake implementations used. | bounds/alignment/order/event tests. | Compiled. | Final gate only. | `include/storage/flash_backend.hpp` |
| 13 | Complete: legal 1-to-0 fake flash with per-type/global/cumulative/address/operation faults. | Host-only by design. | cumulative/address and exhaustive page interruption tests. | N/A. | Final gate only. | `src/storage/fake_flash_backend.cpp` |
| 14 | Complete: deterministic store with owned non-reentrant workspace. | Used at firmware boot and coordinator Save/reset. | positive, empty, alternate, corrupt, fault and reboot tests. | Compiled. | Final gate only. | `src/storage/device_config_store.cpp` |
| 15 | Complete: runtime geometry, linker ASSERT, artifact check, direct guard and real oversized-firmware negative build. | RP backend uses actual `__flash_binary_end`. | both negative scripts. | Oversized real firmware fails before artifacts. | Final gate only. | `src/storage/persistent_region_guard.ld`, `tools/test_oversized_firmware.py` |
| 16 | Complete: per-slot bounds/alignment and SDK `flash_safe_execute`; single-core build is enforced by absence of `pico_multicore`, while SDK refuses unsafe multicore use. | Called only after audio/LED pause. | host unaffected/regression. | Pico Release compiles. | Physical timing only. | `src/storage/rp2040_flash_backend.cpp` |
| 17 | Complete: board adapter, fixed precompute, retained claims, rollback and safe-disable model. | Renderer loads/publishes configuration and reuses PIO/DMA. | `structural_claim_rollback_safe_disable_and_no_leak`. | Concrete renderer path compiles. | Physical boundary observation only. | `src/config/runtime_publication.cpp`, `src/led/diagnostic_renderer.cpp` |
| 18 | Complete: bounded LED boundary and deliberate audio DMA/IRQ pause/resume. | Concrete `FirmwareConfigRuntime`. | timeout/order/metrics fakes. | Concrete Pico path compiles. | Physical interruption duration only. | `src/config/firmware_config_runtime.cpp`, `src/audio/audio_capture.cpp` |
| 19 | Complete: approved prepare/inspect/safe-point/activate/write/commit/inspect/state order. | Firmware constructs coordinator; no excluded editor transport added. | order, timeout and failure-state tests. | Compiled. | Invocation belongs to later approved editor transport. | `src/storage/save_coordinator.cpp` |
| 20 | Complete: independent startup inspection and loaded/factory publication before renderer enable. | Active in `main`. | `startup_load_and_factory_fallback`. | Pico boot path compiled. | Physical boot observation only. | `src/audio/audio_app.cpp`, `src/config/config_boot.cpp` |
| 21 | Complete: confirmation token and same transaction/authority semantics. | Coordinator API constructed in firmware. | wrong token, storage failure, dirty/reload tests. | Compiled. | Physical reset action only. | `src/storage/save_coordinator.cpp` |
| 22 | Complete: bounded boot/fallback/schema/authority/validation/status/timing/drop/bounds summary. | Periodic bounded telemetry. | diagnostics formatting, fallback and timing tests. | Compiled. | Physical observed values only. | `src/config/config_diagnostics.cpp`, `src/config/config_boot.cpp` |
| 23 | Complete: dedicated sixth suite and authoritative verifier/CI entry point. | N/A. | Six suites in four configurations. | Same verifier builds Pico. | Final run. | `tests/host/CMakeLists.txt`, `tools/verify_all.sh` |
| 24 | Complete: corruption, malicious fields, reserved bytes, CRC, interruption and authority simulations. | N/A. | named record/fake/store tests. | N/A. | Final gate only. | `tests/host/device_configuration_tests.cpp` |
| 25 | Complete subject to final rerun. | Features 001–004 preserved. | GCC/Clang/ASan/UBSan six-suite verifier. | Included. | Run final verifier. | `tools/verify_all.sh` |
| 26 | Complete subject to final rerun. | Integrated Feature 005 code, not dead-linked scaffolding. | Host prerequisite. | standard Pico W Release/artifacts. | Run final verifier. | `CMakeLists.txt`, production sources |
| 27 | Complete: linker/image/slots and exact object/workspace/compiler stack frames reported. | Runtime bounds match linker. | memory assertions and `report_config_stack.py`. | size report includes bounds/static symbols. | Capture final numbers. | `tools/report_config_stack.py`, `tools/validate_firmware.py` |
| 28 | Complete software preparation; physical result intentionally open. | Required diagnostics and safe build bounds exist. | build-only as specified. | Release build required. | Owner performs physical procedures. | `specs/005-device-configuration/physical_validation.md` |

## R005-067 named evidence map

- Defaults/eight channels/reserved/current board/no GPIO/metadata: `factory_defaults_all_eight_channels`, `schema_geometry_and_no_gpio`, `exact_golden_bytes_for_every_record_class_and_no_gpio`, `disabled_channel_retained_count_runtime_zero`.
- Validation boundaries (300/301, 800/801, unsupported channels, masks, effects, Idle, audio): `validation_boundaries`, `validation_named_boundaries`, `malformed_boolean_enum_reserved_truncation_and_extra_bytes`.
- Calibration and canonical schema ownership: `canonical_calibration_overlay`, `schema_enum_mapping_is_explicit`, `off_effect_canonicalization_ignores_hidden_fields`, `canonicalization_covers_every_effect_type`.
- Exact bytes/lengths/CRC/reserved/padding/commit: `crc_check_vector`, `codec_roundtrip_and_malformed`, `record_golden_and_selection`, `exact_golden_bytes_for_every_record_class_and_no_gpio`, `record_crc_reserved_and_padding_coverage`.
- Slot selection/target/wrap/duplicate/ambiguity/alternation: `record_golden_and_selection`, `storage_empty_first_and_alternating`, `previous_slot_authority_outcomes`.
- Corruption/interruption/postcommit/reboot/unknown/no repair: `corruption_and_interruption`, `exhaustive_commit_page_interruption_and_reboot`, `post_commit_unknown_does_not_repair`, `previous_slot_authority_outcomes`.
- Safe points/state/reset/runtime resources: `service_safe_points_and_reset`, `activation_then_storage_failure_keeps_active_dirty`, `factory_reset_failure_phases_and_authority`, `structural_claim_rollback_safe_disable_and_no_leak`.
- Boot/fallback/diagnostics/memory/layout: `startup_load_and_factory_fallback`, `diagnostics_named_authority_timing_and_fallback`, `bounded_diagnostics_and_memory_budget`, `flash_layout`, `tools/report_config_stack.py`, `tools/test_persistent_link_guard.py`, and `tools/test_oversized_firmware.py`.

## Completion gate

No software gap is deferred to the owner. The final audit findings were
resolved by field-specific validation diagnostics, post-failure slot
reinspection, exhaustive hidden-field canonicalization checks, production use
of the tested publication coordinator, and Arm compiler/Pico stack-usage call
path analysis. The measured upper bounds are load 2,664 bytes, Save 9,952
bytes, Preview 2,192 bytes, discard 2,192 bytes, and reset 9,960 bytes; the fixed
store workspace is 15,512 bytes and is never automatic storage. The clean
authoritative verifier passed all six suites in all four host configurations,
both linker negatives, stack analysis, and Pico W Release validation. The
independent final audit found no unresolved material issue. Remote
synchronization and final GitHub Actions inspection remain administrative
follow-up. Physical procedures remain unchecked and owner-only.
