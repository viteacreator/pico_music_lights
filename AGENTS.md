# Project Contribution Rules

The detailed engineering rules are in [`docs/DEVELOPMENT_RULES.md`](docs/DEVELOPMENT_RULES.md); they apply together with the focused repository rules below.

## Scope and workflow

- Work is specification-driven. Before implementation, read the applicable
  `requirements.md`, `design.md`, and `tasks.md`: requirements define mandatory
  behaviour, design defines the approved architecture, and tasks define the
  authorized implementation sequence. Resolve conflicts with the owner rather
  than rewriting a specification to match code.
- Implement only approved tasks. A closed Feature is not reopened for optional
  improvements, and work must never expand into the next Feature.
- Classify findings separately as software failures, verification-infrastructure
  failures, owner-reported physical validation, or optional improvements. Only
  the first two can block software completion; never claim physical verification
  without an owner report.
- Prefer small, focused, low-risk changes. Confirm a change remains necessary,
  preserve tested behaviour, review the complete final diff, and remove
  unrelated or generated content.

## Language, real-time, and style

- Firmware and host code uses C++17. Use readable multiline C/C++, one statement
  per line, and format every touched source file with `clang-format` when
  available.
- Do not dynamically allocate in real-time audio, analysis, effect rendering,
  frame preparation, or LED transmission paths. Use fixed-capacity state;
  validate configuration before applying it atomically at a render boundary.
- Reusable effects are hardware-independent, receive an explicit destination
  pixel span, and keep state independent per strip. They consume shared,
  read-only audio/spectrum frames and never embed strip numbers or access
  hardware. Update smoothing once per logical segment or zone per frame, then
  consume the cached value in pixel loops.

## Required verification

- Project-owned code must compile without warnings; GCC and Clang host builds
  use `-Wall -Wextra -Wpedantic -Werror`.
- Run distinct GCC, Clang, GCC AddressSanitizer-only, and GCC
  UndefinedBehaviorSanitizer-only host configurations. Combined sanitizer
  coverage is not a substitute. Every configuration must run all five registered
  host suites.
- Run the standard Pico W Release build with Pico SDK 2.3.0, Arm GNU Toolchain
  15.2.Rel1 (GCC 15.2.1), and picotool 2.3.0. Validate the ELF, UF2, BIN, HEX,
  map, disassembly, metadata, reconstructed images, and deterministic size
  reports; diagnostic/fallback builds and disabled outputs are forbidden.
- The exact full verification command is:

  ```bash
  PICO_SDK_PATH=/absolute/path/to/pico-sdk-2.3.0 tools/verify_all.sh
  ```

- The primary agent owns architecture, integration, complete diff review, the
  firmware build, all host runs, final verification, commits, and reporting.
  Subagents are limited to independent inspection, test-infrastructure review,
  CI review, and final audit; they must not make conflicting repository-wide
  changes.
- **Software-complete** means the approved scope is implemented, every stage of
  the authoritative clean verifier passes, artifacts and sizes are validated,
  the verifier leaves the repository unchanged, all confirmed independent-review
  findings are resolved, CI calls the same entry point, no generated artifacts
  are committed, and no later Feature or existing production behaviour changed.
