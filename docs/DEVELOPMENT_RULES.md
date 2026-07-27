# Development Rules

These rules apply to all AI agents and human contributors working on the Pico Music Lights project.

## Language

* AI agents shall reason and write all project artifacts, code, comments, diagnostics, specifications, and completion reports in English.
* User messages may be written in Romanian; AI agents shall interpret them and respond in English unless the user explicitly requests a translation.

## 1. Spec-driven workflow

* Do not implement significant functionality without approved `requirements.md`, `design.md` and `tasks.md` files.
* Implement only tasks that have been explicitly approved by the project owner.
* Do not implement future tasks while working on the current task.
* Update and review the design before changing an architectural decision.
* Do not modify requirements merely to justify an implementation that does not satisfy them.
* If implementation constraints conflict with the approved requirements or design, report the conflict before proceeding.
* Every completed task shall leave the project compilable.
* Every task shall have a clear and independently verifiable completion condition.
* Hardware-dependent behaviour shall not be reported as verified until the user confirms the physical test.

## 2. Target platform

* Target Raspberry Pi Pico W.
* Use `PICO_BOARD=pico_w`.
* Use the Raspberry Pi Pico SDK.
* Use C++17 for new firmware code.
* Do not use Arduino libraries.
* Do not introduce FreeRTOS unless explicitly required by an approved specification.
* Do not introduce dependencies that are unnecessary for the current task.

## 3. C++ and memory

* Avoid dynamic memory allocation in real-time paths, including audio acquisition, audio processing, effect rendering, LED frame preparation, and LED transmission.
* Do not allocate memory during frame transmission.
* Prefer statically allocated buffers or explicitly managed fixed-capacity storage.
* All important memory limits shall be explicit, documented and verifiable.
* Use fixed-width integer types such as `uint8_t`, `uint16_t` and `uint32_t` where binary size matters.
* Avoid exceptions and RTTI in firmware unless explicitly approved.
* Do not use recursion in real-time or hardware-control paths.
* Use intermediate integer widths large enough to prevent overflow during brightness scaling, colour conversion and timing calculations.
* Validate all array indexes, strip indexes and pixel counts before access.

## 4. Module separation

* Effects shall not directly access GPIO, PIO, DMA, ADC, Wi-Fi, flash, persistent storage, or web-server internals.
* Effects shall consume processed audio information and render logical pixel values.
* The LED driver shall not depend on audio acquisition, audio analysis, effects, web configuration, Wi-Fi, or persistent storage.
* Audio processing shall not depend on strip length or LED hardware.
* Web and configuration modules shall not manipulate hardware registers directly.
* Hardware configuration shall be declared in one clearly identified location.
* GPIO assignments shall not be duplicated throughout the codebase.
* Public interfaces shall expose logical RGBW values.
* Physical channel order shall be handled only by the LED output layer.
* A logical White channel shall never be treated as transparency or alpha.

## 5. Real-time behaviour

* Do not use blocking delays in production audio, effect, or LED-rendering paths.
* Temporary blocking delays are allowed only in isolated hardware-test applications when explicitly documented.
* Networking shall not block audio acquisition, audio processing, or LED rendering.
* Flash writes shall not occur from a real-time processing path.
* Configuration changes shall be validated before becoming active.
* Configuration shared between cores shall use an explicitly thread-safe transfer mechanism.
* Avoid long critical sections and unnecessary interrupt disabling.
* Real-time processing shall have measurable execution time and overrun diagnostics where practical.

## 6. Hardware safety

* Do not start LED strips at high brightness by default.
* Hardware tests shall use a deliberately low initial brightness.
* Do not assume LED-strip voltage from the product name or seller listing.
* Do not assume physical RGBW channel order without a physical test.
* Do not assume the maximum current of one pixel without reliable documentation or measurement.
* LED strips shall not be powered from the Pico W 3.3 V output.
* External LED power and Pico W shall use a common ground.
* Hardware assumptions shall be documented in the relevant requirements or design file.
* Any hardware test shall keep USB serial diagnostics available where technically possible.
* Firmware shall enter or preserve a safe LED-off state after unrecoverable initialization failures.

## 7. PIO and DMA

* PIO programs shall be stored in dedicated `.pio` files.
* Do not duplicate PIO timing constants across multiple modules.
* PIO clock-divider and timing calculations shall be documented.
* PIO block and state-machine allocation shall be explicit.
* DMA-channel allocation shall be explicit.
* PIO and DMA resources shall be checked for allocation failure.
* Do not silently reuse hardware resources already owned by another subsystem.
* Prefer PIO, DMA, timers, ADC FIFO, and other peripherals for deterministic
  or high-rate work when this reduces CPU load without unjustified resource
  complexity. Document resource ownership, timing, failure handling, and
  CPU/peripheral synchronization.
* Changes to PIO timing shall require physical waveform verification before final hardware approval.
* Compilation alone does not validate protocol timing.

## 8. Error handling and diagnostics

* Do not ignore hardware initialization errors.
* Errors shall include sufficient context to identify subsystem, strip index, GPIO, PIO block, state machine, DMA channel, and failure cause when applicable.
* A failure affecting one strip shall not corrupt another strip's buffer.
* A failure affecting one strip should not unnecessarily block unrelated strips.
* Do not use `panic()` for invalid or user-configurable settings.
* Reserve `panic()` for unrecoverable internal programming or platform failures where continuation would be unsafe.
* Invalid configuration shall be rejected or replaced with a documented safe fallback.
* USB serial diagnostics shall remain useful during development.
* Diagnostic logging shall not flood or block real-time processing.

## 9. Build and testing

* Build for `pico_w` after every source-code change.
* Add every new firmware header, translation unit, source file, and PIO source
  to a compile or assembly path in the same task that introduces it.
* Add every new pure-logic test source to the native host-test target in the
  same task that introduces it.
* Fix compilation errors caused by the current change.
* Report unresolved warnings from project-owned code.
* Do not claim that compilation proves correct physical hardware behaviour.
* Hardware tests shall have explicit expected and observed results.
* SK6812 RGBW tests shall independently verify Red, Green, Blue, White, physical channel order, strip index, GPIO output, data direction, and configured pixel count.
* PIO timing shall be checked with a logic analyser or oscilloscope before final protocol validation.
* Temporary tests shall not be removed until their replacement has been built and physically verified.
* Existing working functionality shall not be removed unless the current approved task requires it.

## 10. Generated and external files

* Do not directly modify generated build files.
* Do not modify files inside the Pico SDK.
* Do not copy and modify SDK source code unless the need is explicitly documented.
* Prefer wrapping or configuring external code rather than editing it in place.
* Generated PIO headers shall be produced through CMake.
* Build outputs shall not be committed to Git.
* Credentials, passwords, and private network configuration shall not be committed to Git.

## 11. Code style and maintainability

* Use descriptive names for modules, types, functions, and constants.
* Avoid magic numbers; use named constants.
* Comments shall explain hardware constraints, timing requirements, architectural decisions, and non-obvious trade-offs.
* Comments shall not restate obvious code.
* Keep public interfaces small.
* Prefer strongly typed enumerations for modes, channel orders, states, and errors.
* Avoid global mutable state.
* Clearly define ownership and lifetime of buffers and hardware resources.
* Keep the application entry point focused on initialization and high-level
  orchestration.
* Do not place entire subsystems inside the application entry point.

## 12. AI-agent completion report

After implementing a task, the AI agent shall report:

1. The task that was implemented.
2. Every file that was created or modified.
3. The build command or build operation used.
4. Whether the build succeeded.
5. Any remaining project-owned warnings.
6. Flash and RAM usage when available.
7. Hardware resources used or changed.
8. What was verified through software or compilation.
9. What still requires a physical hardware test.
10. The exact physical test procedure.
11. Any deviation from the approved requirements or design.

The AI agent shall not claim that a physical test passed unless the user explicitly reports that result.
