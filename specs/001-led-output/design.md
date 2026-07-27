# Feature 001 — SK6812 RGBW LED output design

## Scope and inspected baseline

This document designs Feature 001 only. It does not change production code,
`CMakeLists.txt`, or the current `blink.pio` program.

The inspected baseline is the generated Pico SDK Hello World project:

* `CMakeLists.txt` selects `PICO_BOARD pico_w`, sets C++17 (the current source
  is nevertheless C), imports the SDK before `project()`, and creates
  `pico_music_lights` from `pico_music_lights.c`.
* The SDK import resolves `PICO_SDK_PATH` and includes `pico_sdk_init.cmake`.
  The existing build cache resolves SDK 2.3.0 at
  `C:/Users/VicPro/.pico-sdk/sdk/2.3.0`, targeting RP2040.
* The target explicitly enables USB stdio and disables UART stdio. The current
  application calls `stdio_init_all()` and prints `Hello, world!` once per
  second. It also demonstrates DMA, a blink PIO program, interpolation, and a
  timer.
* `blink.pio` is a GPIO blink example, and CMake currently generates only its
  header. It is not an LED protocol implementation.

The proposed source layout is `include/led/` for public interfaces,
`src/led/` for implementations, `src/board/led_board_config.hpp` for the one
GPIO/configuration location, and `pio/sk6812_rgbw.pio` for the later PIO
program. Platform-independent tests live in `tests/host/` with their own native
CMake target. Every task adds each new source, translation unit, header
validation unit, or PIO source to its applicable firmware or host-test build
path immediately; no introduced artifact waits for a later integration task.

## Provisional assumptions and limits

* The required GPIO mapping is GP2, GP3, GP4, GP5, and GP6 for strips 1–5.
  It is declared only in board configuration.
* The physical byte order is not yet confirmed. `RGBW` is the initial test
  default; `GRBW` is also supported. Hardware testing selects the installed
  strip's order, without changing effects.
* The bit period is provisionally 1.25 µs (800 kbit/s), with protocol pulse
  widths encoded in the later PIO source. The reset/latch LOW interval is at
  least 80 µs.
* The approved maximum configured total is 1,200 pixels.
  The maximum remains 300 pixels per strip. Therefore, all five strips may
  individually support up to 300 pixels, but their combined configured count
  shall not exceed 1,200 pixels.
* The specified level shifter, common ground, and externally powered strips
  are mandatory electrical preconditions. Firmware cannot make an unsafe
  power arrangement safe.

## Architecture and ownership

`LedOutputManager` owns exactly five `LedStrip` instances, both static pools,
their lifecycle, resource allocation, validation, diagnostics, and frame
scheduling. Each `LedStrip` owns configuration only: enabled state, pixel
count, physical length/density metadata, brightness, order, reversal, GPIO,
and non-owning slices into the manager-owned logical and packed DMA pools. It
does not own an independent pixel buffer. `Sk6812RgbwDriver` owns PIO program
installation, state-machine setup, DMA-channel ownership, and completion/latch
tracking.

The manager owns two static shared pools with matching slice allocation:

```cpp
std::array<RgbwColor, 1200> logical_pixel_pool;
std::array<uint32_t, 1200> dma_word_pool;
```

At initialization it validates all configurations first, then assigns each
enabled strip one non-overlapping, fixed contiguous slice. A strip's slice
does not move for its initialized lifetime. No strip can access another
slice. Reconfiguration is an explicit stop/validate/reinitialize operation;
it is not permitted while a frame is in flight. There is no heap allocation.

This uses **single logical buffering** plus one static DMA transmission pool.
Before a frame starts, output conversion reads logical pixels and writes
packed, brightness-scaled 32-bit words into matching DMA-pool slices. The
logical pool is protected only during this packing phase. Once packing has
completed and DMA starts, effects may render the next frame into the logical
pool while the packed DMA pool remains immutable for every active DMA transfer.
This transmission pool is not a second logical buffer: it is the DMA source
required to move converted data without consuming CPU time during the waveform.
Double logical buffering is deliberately deferred; it would add a further
4,800 bytes for a 1,200-pixel configuration.

The manager tracks four frame phases:

| Phase | Logical pool | Packed DMA pool | Configuration and new frame start |
| --- | --- | --- | --- |
| `Idle` | renderable | reusable | permitted |
| `Packing` | read-only to the packer | written by the packer | rejected as busy |
| `Transmitting` | renderable for the next frame | immutable for active DMA slices | rejected as busy |
| `Latching` | renderable for the next frame | not reused for a new frame | rejected as busy |

Slice reallocation is also rejected in every non-`Idle` phase. No DMA channel
claiming or other resource allocation occurs in any frame phase.

The LED layer exposes logical pixel operations (`set`, `get`, `fill`, and
`clear`), RGBW types, and non-blocking frame operations equivalent to
`start_show_one()`, `start_show_all_enabled()`, `is_frame_in_progress()`, and
`poll_frame_completion()`. Effects later render into `LedStrip`'s logical API;
audio supplies already processed data to effects; web code turns requests into
validated configuration commands; persistent storage serializes configuration.
None of those layers accesses GPIO, PIO, DMA, or driver timing.

## PIO and concurrent-frame design

Feature 001 uses one conventional serial SK6812 state machine per physical
output — not a single multi-pin packed-parallel state machine. The five
independent state machines run the same PIO program and are started together:

| Strip | GPIO | PIO block | state machine |
| --- | ---: | --- | ---: |
| 1 | GP2 | PIO0 | SM0 |
| 2 | GP3 | PIO0 | SM1 |
| 3 | GP4 | PIO0 | SM2 |
| 4 | GP5 | PIO0 | SM3 |
| 5 | GP6 | PIO1 | SM0 |

The driver loads the PIO program once into each used PIO instruction memory,
claims precisely these state machines, configures each output pin and a
32-bit, MSB-first TX shift register with explicit blocking pulls, and claims
one currently unused DMA channel for each usable strip during initialization.
The claimed channel
number is stored in that strip's driver state and reported in diagnostics; DMA
channel IDs are not hardcoded. One claimed channel feeds its assigned state
machine's TX FIFO with 32-bit words and uses that state machine's PIO TX DREQ
as its pacing request. The DMA source is the corresponding slice of the static
packed transmission pool. No DMA claiming occurs during packing, transmission,
or latching.

For each frame, the CPU converts logical pixels into the DMA pool, then starts
the configured DMA channels and enables the state machines as a concurrent
frame operation. PIO0 SM0–SM3 are synchronized within PIO0. PIO1 SM0 is
started as part of the same frame operation; exact cycle-level phase alignment
between PIO0 and PIO1 is not required, and a small bounded start skew between
the blocks is acceptable. DMA, rather than a CPU polling loop, services the
FIFOs for the full transmission. This gives concurrent waveforms while
allowing each strip to end after its own pixel count. Polling is permitted only
in an isolated single-strip protocol-validation test; it is not the production
frame path.

DMA completion is tracked independently for every enabled strip, but it does
not by itself prove that the final physical bit was transmitted. The PIO
program performs a blocking pull after each 32-bit word. Before each frame, the
driver clears the relevant PIO TX-stall indication. After an enabled DMA
transfer completes, the driver waits for that state machine to stall at its
next blocking pull. This proves that its final word has been shifted out; TX
FIFO empty alone is not accepted as physical completion. The latch timer starts
only after the final enabled state machine has this confirmed completion, then
keeps all lines LOW for at least 80 µs. A shorter strip therefore latches
earlier electrically but receives no additional data; the longest enabled strip
determines the frame's send phase. The implementation uses a monotonic SDK time
source plus DMA and PIO status, not an assumed fixed CPU loop duration, to
enforce final-bit and latch deadlines.

`start_show_one()` and `start_show_all_enabled()` enter `Packing`, then start
the frame without waiting for it to finish. `poll_frame_completion()` is
non-blocking: it advances `Transmitting` to `Latching` only after confirmed PIO
completion and advances `Latching` to `Idle` only after the latch deadline. It
never services TX FIFOs or waits for the full frame. A second frame start,
strip reconfiguration, or slice reallocation while a frame is non-`Idle`
returns a typed busy status. Only the temporary hardware-test application may
use a blocking convenience wrapper.

If either PIO program cannot be installed, a specified state machine is
unavailable, or a DMA channel cannot be claimed, initialization reports
`pio_program_load_failed`, `pio_state_machine_unavailable`, or
`dma_channel_unavailable`, disables only the affected strip(s), and returns
failure to the manager. No partial success may make an uninitialized strip
writable or transmissible. Each strip retains its initialization status. The
manager returns `ok` when all configured strips initialize, `partial_success`
when at least one strip is usable and at least one fails, and `failed` when no
configured strip is usable. Diagnostics name the strip, PIO block, SM, DMA
channel, and GPIO over USB serial.

## RGBW data, order, brightness, and reversal

The only public colour is:

```cpp
struct RgbwColor { uint8_t red, green, blue, white; };
```

`ChannelOrder` is an explicit enum. A pure packing function maps logical
channels to transmission bytes. With MSB-first shifting, initial mappings are
`RGBW -> R<<24 | G<<16 | B<<8 | W` and
`GRBW -> G<<24 | R<<16 | B<<8 | W`. Adding a verified order means adding one
mapping entry and test coverage; it never changes `RgbwColor`, effects, or
application calls.

Per-strip brightness is an 8-bit factor. Just before packing, every logical
channel is independently scaled with a widened calculation, for example
`(static_cast<uint16_t>(channel) * brightness + 127u) / 255u`. This rounds to
the nearest 8-bit result while preserving zero and full brightness exactly.
The logical pool remains
unchanged. White is scaled identically to RGB, can be set alone or alongside
RGB, and is never treated as alpha or generated automatically from RGB.
Automatic RGB-to-white conversion remains outside Feature 001.

Logical index `i` maps to pool-slice index `i` normally, or `count - 1 - i`
when `reversed` is true. This mapping occurs when obtaining the next logical
pixel for packing; it reverses visual direction without changing the GPIO,
PIO program, electrical data direction, or effect algorithm.

## Capacity, validation, and static RAM budget

All limits are compile-time constants in the board/LED configuration header:
`kStripCount = 5`, `kMaxPixelsPerStrip = 300`, and
`kMaxConfiguredPixels = 1200`. Initialization rejects an enabled strip with
zero pixels, a count above 300, an unsupported channel order, duplicate GPIO,
or a sum above 1,200. It also rejects invalid strip/pixel indexes and all
operations on uninitialized strips. Public operations return a typed status;
the temporary application logs it on USB. Failed validation leaves previously
valid allocations untouched.

| Item | Formula | Static reserved RAM | Used at 1,200 configured pixels |
| --- | --- | ---: | ---: |
| Logical RGBW pool | 1,200 × 4 bytes | 4,800 B | 4,800 B |
| Per-strip configuration/state | 5 × `sizeof(LedStrip)` | implementation-dependent | 5 instances |
| PIO TX FIFOs | hardware-resident | 0 B SRAM | hardware only |
| Packed DMA-word pool | 1,200 × 4 bytes | 4,800 B | 4,800 B |
| DMA channel configuration/state | 5 channels | implementation-dependent | 5 channels |
| Double buffer | not selected | 0 B | 0 B |

One 300-pixel strip consumes 1,200 bytes in each pool, or 2,400 bytes total.
The two 4,800-byte pools support 1,200 configured pixels, not all five
300-pixel maxima (which would require 1,500 pixels / 12,000 bytes across both
pools). `sizeof(LedStrip)` and manager/driver/DMA bookkeeping will be asserted
and reported after implementation; they are intentionally not guessed here.
The LED data has a clear fixed reservation of 9,600 bytes and no per-frame
allocation.

## Pure-logic test mechanism

Pure LED logic is built by a native host-side CMake target in `tests/host/`.
It compiles only platform-independent LED code and requires neither the Pico
SDK runtime nor connected LED hardware. The target verifies RGBW/GRBW packing,
brightness scaling, stable logical indexing, reversed output mapping,
pixel-count validation, shared-pool slice allocation, non-overlap, and typed
status behaviour. Every pure source file is added to this target in the same
task in which it is introduced and is also added to the Pico firmware target
when it belongs to firmware.

## Timing model

At 800 kbit/s, one 32-bit RGBW pixel takes 40 µs. A strip send phase is
`pixel_count × 32 / 800,000` seconds, followed by at least 80 µs LOW. Examples:

| Pixels | Data time | Minimum total including latch |
| ---: | ---: | ---: |
| 1 | 40 µs | 120 µs |
| 300 | 12.00 ms | 12.08 ms |
| longest enabled strip N | N × 40 µs | N × 40 µs + 80 µs |

Because sends are concurrent, five unequal strips do not sum their durations;
the longest enabled strip sets the minimum complete-frame duration. Actual PIO
cycle timing will be derived from the configured system clock and documented
beside the PIO timing constants when the PIO file is implemented.

## Build integration, deferred implementation

Each implementation task adds its new LED C++ source, public include path,
compile-validation translation unit, or host-test source to its applicable
build target immediately. The task that adds `pio/sk6812_rgbw.pio` also adds
`pico_generate_pio_header(... pio/sk6812_rgbw.pio)` while retaining existing
blink-header generation. The target retains `hardware_pio`, `hardware_dma`,
and `pico_stdlib`. Task 15 removes obsolete blink-example dependencies only
after the replacement application builds. No CMake or PIO change is made by
this design task.

## Physical test procedure and temporary application

With shared ground, level shifting, and external LED power verified, configure
safe low brightness and tested counts for all five strips. The temporary
hardware-test application will:

1. call `stdio_init_all()` before LED initialization and print each strip's
   index, GPIO, count, order, reversal, and init result over USB;
2. initialize all five outputs, then identify one strip at a time;
3. for each strip, display Red, Off, Green, Off, Blue, Off, Neutral White,
   Off at low brightness, recording observed colour and whether the selected
   physical strip is correct;
4. use the observations to select RGBW or GRBW (or a later verified order),
   repeat the colour sequence, and confirm the dedicated white LED is neutral
   white rather than RGB mixing;
5. light a distinctive first-pixel marker followed by a different final-pixel
   marker, once normal and once reversed, to verify physical data direction;
6. configure deliberately different valid counts and verify the marker ends
   at each physical strip end, including a short strip completing before a
   long strip; and
7. finish with five different low-level RGBW fills, including white-only and
   combined RGBW, while continuing periodic USB status/error prints.

The identification delays are controlled blocking delays only in this test
application. USB CDC remains configured because the existing target setting
(`pico_enable_stdio_usb(... 1)`) is preserved, `stdio_init_all()` remains at
startup, and diagnostic output continues before, during, and after test
sequences. Normal rendering will use the non-blocking driver frame state
machine instead.
