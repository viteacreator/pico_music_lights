# Reproducible verification

`tools/verify_all.sh` is the sole authoritative full verification entry point.
It performs clean GCC, Clang, ASan-only, UBSan-only, and Pico W Release builds;
checks all five host suites explicitly; and validates every firmware artifact.

## Prerequisites

Provide Pico SDK 2.3.0 through `PICO_SDK_PATH`. `PATH` must select GCC and
Clang host compilers, CMake, Ninja, Python 3, Git, ripgrep, Arm GNU Toolchain 15.2.Rel1
(GCC 15.2.1), and picotool 2.3.0. The verifier prints and validates the relevant
versions and refuses SDK, board, toolchain, picotool, UF2-disable, or fallback
mismatches.

From any working directory, run:

```bash
PICO_SDK_PATH=/absolute/path/to/pico-sdk-2.3.0 /path/to/pico_music_lights/tools/verify_all.sh
```

There are no alternative commands that count as full verification. Developers
may run ordinary CMake/CTest commands while diagnosing a failure, but completion
requires the command above.

## Outputs and failures

The verifier requires a clean Git working tree. By default, builds and reports
are placed in a newly created directory under `${TMPDIR:-/tmp}`. Set
`VERIFY_OUTPUT_DIR` to a nonexistent path outside the repository to retain
results at a known location (as CI does); existing paths are rejected so every
configuration is clean.
No build occurs in the source tree. Each stage prints `PASS` or terminates
nonzero; `verification-summary.txt` ends in `verification=PASS` only after
repository-integrity validation.

Expected firmware outputs are `pico_music_lights.elf`, `.uf2`, `.bin`, `.hex`,
`.map`, and `.dis`, plus `firmware-size-report.txt`. Verification fails for a
missing/skipped suite, warning, sanitizer diagnostic, command error, incorrect
version/configuration, invalid or inconsistent artifact, incomplete size data,
or any source-tree change made during the run.

GNU `size` reports `text` (code/read-only data), `data` (initialized writable
storage, occupying both load image and RAM), `bss` (zero-initialized RAM), and
their GNU total. The separate address-classified totals sum allocatable ELF
sections located in RP2040 flash and RAM address ranges. These classifications
answer different questions and therefore are reported independently. Largest
flash and RAM symbols help explain their respective totals.

GitHub Actions installs the pinned SDK/toolchain/picotool inputs, invokes this
same script with a retained external output directory, and uploads the summary,
firmware artifacts, and size report. Generated firmware is never committed.
