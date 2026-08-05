# Feature 005 — Ordered Implementation Tasks

No task is implemented by this specification package. Every implementation task must leave the repository compilable, must not add Wi-Fi/web/JSON/OTA/USB configuration protocol scope, and must preserve Features 001–004 except where an approved task explicitly integrates loaded configuration.

1. Public configuration types and fixed limits. Scope: add `config/device_config.*` with fixed limits, LED/effect/Idle/audio calibration value types, result enums, and static assertions against board/effect strip counts. Likely files: new `include/config/`, `src/config/`, host CMake. Tests: compile-only and field-limit host tests. Completion: Pico and host builds compile. Dependencies: none.

2. Factory defaults. Scope: add factory construction for six GP2–GP7 strips, 132/174/141/81/96/72 pixels, GRBW, enabled, unreversed, brightness 16, density 60, six Gyver VU defaults, disabled Idle, canonical audio floors. Tests: host assertions for every default field. Completion: defaults match requirements and no startup behavior changes. Dependencies: task 1.

3. Configuration validation. Scope: implement whole-config and preview-subset validation for strip count, GPIO mapping, duplicate mappings, pixel limits, brightness/order enums, effects, Idle, audio floors, and overflow. Tests: positive, boundary, malicious-field, invalid enum/source/pixel/GPIO/Idle/audio cases. Completion: all invalid cases return typed diagnostics without side effects. Dependencies: tasks 1–2.

4. Draft, active, and persisted state management. Scope: add `ConfigService` state ownership, dirty tracking, reload/discard, and all-or-nothing publication stubs that initially use factory defaults. Tests: state-transition host tests. Completion: service compiles and preserves current runtime until later integration tasks. Dependencies: task 3.

5. Explicit serialization and deserialization. Scope: add deterministic little-endian schema-1 codec with fixed buffers and stable enum ids. Tests: round trip, known byte order, truncation, length mismatch, malformed bool/enum, integer-overflow safety. Completion: no raw struct-memory persistence. Dependencies: task 3.

6. CRC32. Scope: add standalone CRC32 implementation. Tests: known vectors, incremental vs one-shot, empty payload. Completion: usable by codec/record tests. Dependencies: none.

7. Slot format and slot inspection. Scope: define header, commit marker, payload/header CRC checks, erase-state handling, and per-slot diagnostic summary. Tests: valid slot, erased, bad magic, bad commit, partial commit marker, illegal marker transition, bad header CRC, bad payload CRC, bad length, unsupported schema. Completion: host inspection classifies every case. Dependencies: tasks 5–6.

8. Newest-valid-slot selection. Scope: implement two-slot selection and sequence wrap comparison. Tests: one invalid/one valid, both invalid, both valid A newer, B newer, duplicate sequence, wrap `0xFFFFFFFF -> 0`, ambiguous half-range. Completion: selected source and fallback reason are deterministic. Dependencies: task 7.

9. Interrupted and corrupt write handling model. Scope: specify and implement write transaction state machine independent of hardware backend. Tests: interrupted erase, interrupted program before payload complete, interrupted commit marker, corrupt padding, previous valid recovery. Completion: fake transactions prove new slot is not authoritative until committed. Dependencies: tasks 7–8.

10. Flash backend abstraction. Scope: add read/erase/program/bounds interface with alignment and region descriptors. Tests: fake backend rejects misalignment/out-of-range and records operations. Completion: storage code depends only on abstraction. Dependencies: task 9.

11. Host fake-flash backend. Scope: implement erase-state array, programmable 1-to-0 semantics, injected failures and interruption points. Tests: corruption and power-interruption simulations. Completion: all storage edge tests run without Pico SDK. Dependencies: task 10.

12. Device configuration store. Scope: combine codec, records, selection, fake backend writes, verification readback, and persisted-state update. Tests: save/load positive, fallback to defaults, both slots invalid, unsupported schema, CRC/length mismatch. Completion: host store is deterministic. Dependencies: tasks 5–11.

13. Persistent region layout checks. Scope: add compile-time constants and runtime validation hooks for flash size, sector alignment, slot placement, linker image end, and reserved areas, plus build/link-time reservation or size assertion. Tests: host flash-overlap and alignment cases. Completion: overlap is refused before slot access and the Pico build fails if the image would grow into the reserved region. Dependencies: task 10.

14. Pico SDK flash backend. Scope: implement RP2040 read/erase/program backend using SDK alignment, RAM-resident critical flash calls where required, and no real-time invocation. Tests: compile Pico Release; host remains unaffected. Completion: backend builds for Pico W and is not called from ISR/render paths. Dependencies: tasks 10 and 13.

15. Controlled runtime activation. Scope: publish previewable non-structural changes through EffectEngine/Idle/audio calibration at render/audio-safe boundaries and structural LED changes through two-phase reinitialization/restart boundary. Tests: atomic success/failure, structural-not-mid-frame, partial reinitialization failure, dirty tracking. Completion: invalid publication leaves active config unchanged. Dependencies: task 4.

16. Controlled save integration. Scope: add save coordinator that uses controlled activation when needed, reaches LED-frame boundary, prevents new rendering, handles audio capture/processing deliberately, performs flash transaction, verifies, restores runtime, and records duration/drop diagnostics. Tests: host coordinator simulation plus Pico build, including activation-success/write-failure dirty semantics. Completion: save can only be requested explicitly and all save failure phases match the design. Dependencies: tasks 12, 14, and 15.

17. Boot integration. Scope: normal startup loads newest valid supported config or factory defaults before renderer enable; diagnostic scenes do not auto-run in Release. Tests: host boot policy and Pico build. Completion: current factory behavior is reproduced when slots are empty, with fallback diagnostics. Dependencies: tasks 12, 14, and 16.

18. Factory reset. Scope: explicit confirmation API, factory activation, persistent reset write, and reset diagnostics. Tests: wrong confirmation rejected, confirmed reset restores defaults, reset write failure distinguishes active vs persisted state. Completion: no accidental reset path. Dependencies: tasks 16–17.

19. Bounded diagnostics. Scope: add fixed summary of boot source, slot states, dirty flag, validation error, save/reset status, duration, dropped frames/audio interruption, schema, flash bounds. Tests: formatting/bounds host tests if formatting is project-owned. Completion: Release diagnostics are bounded; development USB helpers disabled by default. Dependencies: tasks 16–18.

20. Focused host tests and suite registration. Scope: add or extend host suites for config validation, codec, storage, fake flash, and service lifecycle. Tests: run all registered host suites under GCC, Clang, GCC ASan-only, and GCC UBSan-only. Completion: all five existing suites plus any new suite run in every configuration. Dependencies: tasks 1–19.

21. Corruption and power-interruption simulations. Scope: expand fake-flash scenarios for positive, boundary, corruption, truncation, sequence-wrap, unsupported-schema, both-slots-invalid, interrupted-erase, interrupted-program, CRC mismatch, length mismatch, malicious-field, and flash-overlap cases. Tests: named regression cases for each category. Completion: every requirement test category is covered. Dependencies: tasks 11–20.

22. Regression verification. Scope: run the authoritative host configurations and ensure Features 001–004 tests still pass. Tests: GCC, Clang, GCC AddressSanitizer-only, GCC UndefinedBehaviorSanitizer-only host configurations, each running all registered suites. Completion: warning-clean host results. Dependencies: tasks 20–21.

23. Pico W Release build. Scope: build standard Release firmware with Pico SDK 2.3.0, Arm GNU Toolchain 15.2.Rel1, `PICO_BOARD=pico_w`, and picotool 2.3.0. Tests: `PICO_SDK_PATH=/absolute/path/to/pico-sdk-2.3.0 tools/verify_all.sh`. Completion: ELF, UF2, BIN, HEX, map, disassembly, metadata, reconstructed images, and deterministic size reports validated. Dependencies: task 22.

24. Firmware-size reporting. Scope: report application image end, persistent-region bounds, two slot sizes, flash/RAM usage, and static serialization buffers. Tests: verifier size output and map inspection. Completion: report proves no image/persistent overlap and no generated artifacts are committed. Dependencies: task 23.

25. Physical-validation preparation. Scope: prepare a hardware test build or instructions without enabling hidden config protocols; document expected diagnostics for boot, save, interrupted write, reset, and calibration persistence. Tests: build only; no physical pass claimed without owner report. Completion: `physical_validation.md` procedure is ready for owner execution. Dependencies: tasks 23–24.
