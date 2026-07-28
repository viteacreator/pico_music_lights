# Feature 001 — implementation tasks

No task below is implemented by this planning change. Every task has one clear
responsibility, compiles or assembles every artifact it introduces in that same
task, and ends with a clean Pico W build. Pure logic is also built and run by
the native host-test target where applicable.

1. Add `RgbwColor` and `ChannelOrder` public headers, a compile-validation
   translation unit, and a native `tests/host/` CMake target with a header-only
   test. Add the translation unit to the Pico target and the test source to the
   host target in this task. Verify: the Pico W build and native host test both
   succeed.

2. Add the board LED configuration header declaring GP2–GP7, six strips,
   the 300-pixel per-strip limit, the 800-pixel total limit, and default
   density. Update the already compiled validation translation unit to include
   it. Verify: the Pico W build succeeds and a compile-time check confirms six
   unique GPIO values.

3. Add typed LED status and initialization-result definitions, including
   `ok`, `partial_success`, `failed`, invalid index/count/order, unavailable
   PIO/DMA resource, uninitialized use, and busy status. Update the compiled
   validation translation unit and host test in this task. Verify: both builds
   succeed and the host test verifies the typed-result semantics.

4. Add the platform-independent RGBW conversion source file and add it to both
   the Pico firmware target and native host-test target in this task. Implement
   RGBW/GRBW packing and four-channel brightness scaling. Verify: both builds
   succeed and host tests cover packing, zero/full/intermediate brightness, and
   unchanged source colours.

5. Add `LedStrip` public/private source files and add every new `.cpp` file to
   the Pico and host-test targets in this task. Implement configuration
   validation and manager-assigned non-owning slice binding. Verify: both
   builds succeed and host tests reject zero, over-300, and unsupported-order
   configurations.

6. Implement `LedStrip` logical pixel set/get/fill/clear operations without
   applying reversal to buffer storage. Update the existing host test in this
   task. Verify: both builds succeed; logical indexes remain stable with
   `reversed` enabled or disabled; and out-of-range access is rejected.

7. Add the platform-independent output-conversion source file and add it to
   both the Pico and host-test targets in this task. Implement reversed
   logical-to-physical mapping into packed transmission words. Verify: both
   builds succeed and host tests cover normal/reversed output order without
   modifying the logical pool.

8. Add `LedOutputManager` public/private source files and add every new `.cpp`
   file to both the Pico and host-test targets in this task. Implement manager
   ownership of static 800-pixel logical and 800-word packed pools,
   validated fixed slice allocation, and non-overlap. Verify: both builds
   succeed and host tests reject an 801-pixel aggregate and overlapping slices.

9. Add `pio/sk6812_rgbw.pio` with centralized timing constants and add
   `pico_generate_pio_header()` for it in CMake in this task. Retain
   `blink.pio` and its generated header. Verify: the clean Pico W build
   assembles both PIO programs and generates both headers.

10. Add the SK6812 driver public/private source files and add every new `.cpp`
    file to the Pico target in this task. Implement program loading, GP2–GP7
    setup, PIO0 SM0–SM3 and PIO1 SM0–SM1 allocation, and per-strip initialization
    status. Verify: the Pico W build succeeds and diagnostics distinguish
    `ok`, `partial_success`, and `failed` without marking an uninitialized
    strip usable.

11. Implement DMA initialization: claim one currently unused DMA channel for
    each usable strip, store its claimed number in driver state, configure its
    PIO TX DREQ, and report it through USB diagnostics. Do not hardcode DMA
    channel IDs or claim channels during frame processing. Verify: the Pico W
    build succeeds and allocation failure yields accurate per-strip status.

12. Implement a single-strip polling transmission path only for PIO protocol
    validation. Verify: the Pico W build succeeds; a logic analyser shows 32
    bits per pixel, nominal bit timing, and at least 80 µs LOW reset; and this
    path is not used by production frame transmission.

13. Implement concurrent six-strip DMA transmission using one claimed DMA
    channel per enabled strip. Pack logical RGBW slices into the static
    `uint32_t` transmission pool before starting DMA; keep active packed slices
    immutable while DMA is active. Synchronize PIO0 SM0–SM3 within PIO0 and
    start PIO1 SM0–SM1 as part of the same frame operation with documented bounded
    inter-block skew. Verify: the Pico W build succeeds; unequal strips overlap
    in time; shorter strips receive exactly their configured word count; and
    USB diagnostics remain responsive without CPU FIFO polling.

14. Implement the asynchronous frame state machine and public API:
    `start_show_one()`, `start_show_all_enabled()`, `is_frame_in_progress()`,
    and `poll_frame_completion()`. Implement `Idle`, `Packing`,
    `Transmitting`, and `Latching` phases. Confirm physical completion only
    when each enabled state machine stalls at its next blocking pull after DMA
    completion; do not treat TX FIFO empty alone as sufficient. Start the latch
    timer only after the final confirmed PIO completion. Verify: the Pico W
    build succeeds; start returns without waiting for the full frame; logical
    rendering is possible after packing; active DMA slices remain immutable;
    new starts/reconfiguration/slice reallocation return busy until `Idle`; and
    completion becomes visible only after PIO completion and latching.

15. Replace the Hello World/DMA/blink demonstration application with the
    temporary LED hardware-test application, retaining `stdio_init_all()` and
    continuous USB diagnostics. Remove `blink.pio`, its generated-header
    integration, and obsolete demonstration dependencies only after the
    replacement application builds. Verify: the clean Pico W build succeeds
    and USB prints every strip's configuration and initialization status.

16. Implement the low-brightness per-strip identification sequence: Red, Off,
    Green, Off, Blue, Off, Neutral White, Off. Restrict blocking delays to this
    temporary hardware-test application. Verify: the Pico W build succeeds and
    USB logs every sequence phase.

17. Perform and record the physical channel-order, dedicated-white, direction,
    GPIO-output, and unequal-length procedure from `design.md`. Verify: the
    clean Pico W build succeeds and the user records expected and observed
    results for all six strips while USB serial remains active.

18. Add final RAM and hardware-resource diagnostics. Add compile-time
    assertions for `sizeof(RgbwColor) == 4`, strip count, maximum pixels per
    strip, maximum total pixels, logical-pool capacity, and packed-pool
    capacity. Inspect the complete send path to confirm it contains no `new`,
    `delete`, `malloc`, `calloc`, `realloc`, or per-frame dynamic allocation.
    Report logical-pool RAM, packed-pool RAM, manager/strip object RAM, PIO
    programs, PIO state machines, claimed DMA channels, and maximum frame
    duration. Verify: the Pico W build and native host test succeed.
