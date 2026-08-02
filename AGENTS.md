# Project Contribution Rules

- Use conventional readable multiline C and C++ formatting: one statement per
  line, with normal multiline functions, classes, conditions, and loops.
- Format every touched source file; use `clang-format` when available.
- Use subagents only for independent responsibilities. The primary agent owns
  integration, complete review, firmware build, and host-test run.
- Do not claim physical verification unless the project owner reports it.
- Prefer small, focused, low-risk changes; avoid broad refactoring unless required.
- Confirm a change is still necessary before editing, preserve tested behaviour,
  review the complete final diff, and remove unrelated/generated changes.
- Keep reusable effect calculations hardware-independent and pass each effect an
  explicit destination pixel span.
- Keep effect state independent for every strip. Effects consume shared,
  read-only audio and spectrum frames; they must not embed physical strip
  numbers or access hardware.
- Update temporal smoothing state once per logical source segment or zone per
  rendered frame. Pixel loops must consume the cached result, so response does
  not depend on strip length.
- Validate effect configuration changes before applying them atomically at a
  render boundary. Do not allocate memory dynamically in the real-time render
  path.
