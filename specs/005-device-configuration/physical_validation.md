# Feature 005 — Physical Validation Plan

This document describes only validation that requires real Raspberry Pi Pico W hardware. No Feature 005 physical validation has been performed or claimed.

## Required hardware setup

- Raspberry Pi Pico W running a Feature 005 implementation build.
- The established six SK6812 RGBW strips connected to GP2–GP7 with common ground and external LED power.
- USB serial available for bounded diagnostics when a diagnostic build explicitly enables it.
- A safe way to power-cycle or reset the Pico W during flash erase/program tests.
- Optional logic analyser or oscilloscope for confirming that LED output is quiescent during controlled save and resumes afterward.

## Result-recording rule

Each procedure below requires the owner to record: build identifier, build type, starting persistent-slot state, action performed, expected diagnostic fields, observed diagnostic fields, expected visible LED behavior, observed visible LED behavior, pass/fail result, and notes. This document does not contain observed results.

## Hardware-only procedures

1. Boot-load validation. Preconditions: Release build; both persistent slots erased by a known flashing or diagnostic erase step. Action: boot the device. Expected diagnostics: boot source `factory_defaults`, both slots erased or invalid, dirty false, no save in progress. Expected LEDs: current six installed channels show factory Gyver VU output using the factory layout after audio starts; reserved channels 6 and 7 remain disabled in diagnostics; no automatic temporary diagnostic-scene cycling.

2. Persisted-load validation. Preconditions: one valid saved profile with a visible non-structural difference such as lower brightness or a changed Gyver colour palette. Action: power-cycle. Expected diagnostics: selected newest valid slot and sequence, boot source `persistent`, dirty false. Expected LEDs: rendering matches the saved profile, not factory defaults.

3. Structural LED boundary validation. Preconditions: diagnostic implementation build with an explicit owner-triggered structural-preview test and visible steady output. Action: request a structural change such as disabling one strip or reducing its pixel count, then observe one complete frame before the controlled reinitialization/restart boundary and one after. Expected diagnostics: change accepted into draft, not active before boundary, reinitialization boundary entered, active generation changes only after boundary. Expected LEDs: no mid-frame tearing or partial length change before boundary; after boundary the affected strip is safely off or uses the new length.

4. Interrupted-erase validation. Preconditions: two slots where one slot contains a known previous valid profile. Action: use a diagnostic build with a named `before_target_erase` or `during_target_erase` injection pause, then remove power at that point. Expected diagnostics after reboot: previous valid slot selected, target slot erased or corrupt, fallback not needed. Expected LEDs: previous profile is active.

5. Representative save power-interruption validation. Preconditions: one previous valid slot. Action: perform repeated owner-controlled power removals during Save, using diagnostic injection pauses when available at representative payload-program and commit-page phases. Expected diagnostics after reboot: either the previous valid slot is selected or a fully committed new slot is selected; an erased, partial, malformed, or inconsistent target slot is rejected. Expected LEDs: selected profile matches diagnostics. Exhaustive interruption at every byte or internal position of a 256-byte flash program operation remains a host fake-flash requirement, not a required owner hardware procedure. Exact internal hardware interruption testing is optional when dedicated external hardware supports it.

6. Factory-reset validation. Preconditions: a valid non-default profile is active and persisted. Action: perform the explicit confirmation mechanism. Expected diagnostics: reset confirmed, factory defaults activated, reset record committed and verified, dirty false. Expected LEDs: factory layout and factory Gyver VU defaults are active. After power-cycle, expected diagnostics still show factory defaults loaded from a valid persisted record or equivalent reset source.

7. Save-interruption observability. Preconditions: diagnostic or Release build with bounded save diagnostics visible. Action: perform a normal Save without interruption. Expected diagnostics: save duration is finite and within the implementation's specified bound, dropped/interrupted audio and LED-frame counters are reported as deltas, and diagnostics remain bounded. Expected LEDs: rendering resumes automatically after save.

8. Valid flash-region reporting validation. Preconditions: standard valid Release build only; intentionally overlapping configurations are build/link-time negative tests and must never be flashed to hardware. Action: boot and inspect bounded diagnostics or generated validation report. Expected diagnostics: application image bounds and persistent-region bounds are present, sector-aligned, within physical flash, and non-overlapping. Expected LEDs: normal saved-or-factory profile operation.

9. Normal Release startup validation. Preconditions: standard Release build. Action: boot with USB attached and then with USB disconnected. Expected diagnostics/behavior: no USB configuration protocol is required or exposed, development command helpers are disabled by default, no Wi-Fi/AP/HTTP/web/OTA activity starts, and temporary diagnostic scenes do not auto-run.

10. Audio-calibration persistence validation. Preconditions: quiet input suitable for safe calibration. Action: perform a safe noise-floor calibration through the approved explicit mechanism, Save, then power-cycle. Expected diagnostics: saved Gyver VU floors/hysteresis and spectrum thresholds reload from persistent storage. Expected LEDs: quiet-input Gyver VU gates remain closed according to saved floors, and ordinary music opens the gates.

11. LED factory-layout validation. Preconditions: confirmed factory reset. Action: inspect startup diagnostics and visible output. Expected diagnostics: logical channels 0–5 map through board configuration to GP2–GP7, use 132/174/141/81/96/72 pixels, GRBW order, enabled, no reversal, brightness 16/255, density 60, and unknown length; logical channels 6–7 are disabled with zero pixels, Effect Off, unknown length, unknown density, and no stored GPIO. Expected LEDs: every currently installed physical strip responds on its board-assigned GPIO and no reserved channel is driven.

Physical validation is complete only after the owner reports observed results for the relevant procedures. Compilation or host tests do not prove physical flash timing, power-interruption behavior, LED electrical behavior, or startup observations.
