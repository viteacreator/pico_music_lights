# Feature 001 — SK6812 RGBW LED Output

## Goal

Implement the initial addressable LED output subsystem for five independent SK6812 RGBW LED strips.

This feature shall provide:

* the low-level SK6812 RGBW transmission driver;
* RGBW pixel-buffer management;
* five independent physical LED outputs;
* static colour tests;
* dedicated White-channel tests;
* strip identification tests.

Music processing, web configuration and persistent storage are excluded from this feature.

## Target hardware

* Board: Raspberry Pi Pico W
* MCU: RP2040
* LED type: SK6812 RGBW
* LED package: SMD5050
* LED density: 60 individually addressable pixels per metre
* Colour channels: Red, Green, Blue and Neutral White
* Nominal data rate: approximately 800 kbit/s
* Data size: 32 bits per pixel
* Number of physical outputs: 5

## Initial GPIO assignment

* Strip 1: GP2
* Strip 2: GP3
* Strip 3: GP4
* Strip 4: GP5
* Strip 5: GP6

The GPIO assignment shall be declared in one board-configuration location.

GPIO numbers shall not be duplicated throughout the implementation.

## Electrical assumptions

* The LED strips use an external power supply matching the actual strip voltage.
* The strip voltage shall be physically verified before connection.
* The Pico W and LED power supply shall share a common ground.
* A suitable logic-level buffer shall be used between the Pico W and LED data inputs.
* LED-strip power shall not be supplied by the Pico W 3.3 V output.
* Firmware tests shall initially use low brightness.
* The firmware shall not assume a fixed maximum current per pixel until the actual strips are measured or their reliable electrical specification is available.

## SK6812 data protocol

Each physical pixel requires four 8-bit channel values:

* Red;
* Green;
* Blue;
* White.

The driver shall transmit exactly 32 data bits for every configured pixel.

The nominal bit period is approximately 1.25 microseconds.

After transmitting a complete strip frame, the data line shall remain LOW for a sufficient reset/latch interval.

The initial minimum reset interval shall be 80 microseconds.

Protocol timing constants shall be defined in one driver-specific location.

## Channel order

The logical pixel representation shall always use:

```cpp
RgbwColor {
    red,
    green,
    blue,
    white
}
```

The logical representation shall remain independent from the physical transmission order.

The physical order shall be configurable.

Initial supported physical orders shall include at least:

* RGBW;
* GRBW;
* RGBW variants required after hardware testing.

The exact physical order of the user’s strips shall be determined through a hardware test.

Changing the physical order shall not require modifications to effects or application logic.

## Pixel representation

A pixel shall not be represented as RGB or RGBA.

The fourth byte is a physical White-light intensity, not transparency.

A suitable public representation is:

```cpp
struct RgbwColor {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t white;
};
```

The implementation may use packed transmission words internally, but public interfaces shall clearly preserve the four logical channels.

## White-channel behaviour

The White channel shall be independently controllable.

The implementation shall support:

* White only;
* RGB only;
* RGB and White simultaneously;
* all channels off;
* independent brightness scaling of the complete RGBW pixel.

The driver shall not automatically replace RGB values with White values.

Automatic RGB-to-RGBW conversion is outside the scope of this feature.

Later effects may explicitly decide how much energy is assigned to RGB and how much is assigned to the dedicated White channel.

## Strip configuration

Every strip shall have:

* enabled state;
* pixel count;
* physical length;
* LED density;
* brightness;
* channel order;
* reversed state;
* RGBW pixel-buffer slice.

The initial density shall default to:

```text
60 pixels per metre
```

The firmware shall operate using an integer pixel count.

Physical length is configuration metadata used to calculate or display the pixel count.

The relationship is:

```text
pixel count = strip length in metres × 60
```

The web interface may later allow either:

* direct pixel-count entry; or
* length entry with automatic pixel-count calculation.

## Required driver functionality

The initial implementation shall support:

* initializing one strip;
* initializing all five strips;
* enabling and disabling a strip;
* clearing one strip;
* clearing all strips;
* setting one RGBW pixel;
* reading one RGBW pixel from the logical buffer;
* filling a strip with one RGBW colour;
* setting White-only output;
* applying global strip brightness;
* transmitting one strip;
* transmitting all enabled strips;
* identifying one strip;
* validating pixel-count limits.

Production transmission shall provide non-blocking operations equivalent to:

* `start_show_one()`;
* `start_show_all_enabled()`;
* `is_frame_in_progress()`;
* `poll_frame_completion()`.

Starting a new frame or reconfiguring a strip while a frame is active shall
return a typed busy status. A blocking convenience wrapper is allowed only in
the dedicated temporary hardware-test application.

## Brightness handling

Brightness shall be applied to all four channels:

* Red;
* Green;
* Blue;
* White.

Brightness calculation shall use an intermediate integer width large enough to prevent overflow.

Brightness shall not modify the original logical pixel value unless explicitly documented.

The design shall state whether brightness is applied:

* while rendering into the logical buffer; or
* while converting the logical buffer into transmission data.

The preferred design is to preserve the original logical pixel values and apply brightness during output conversion.

## Identification mode

The function `identifyStrip()` shall make one selected physical strip clearly identifiable.

The sequence shall use low brightness and shall perform:

1. Red only;
2. Off;
3. Green only;
4. Off;
5. Blue only;
6. Off;
7. Neutral White only;
8. Off.

This test shall verify:

* the correct physical strip;
* the data direction;
* Red-channel mapping;
* Green-channel mapping;
* Blue-channel mapping;
* White-channel mapping;
* physical channel order.

The identification sequence may initially use controlled blocking delays in a dedicated hardware-test application.

Normal production rendering shall not depend on blocking delays.

## Initial test application

Until configuration and web control are implemented, the main application shall run a temporary hardware test.

The test shall:

1. initialize all five outputs;
2. print the configured GPIO and pixel count for every strip;
3. identify Strip 1;
4. identify Strip 2;
5. identify Strip 3;
6. identify Strip 4;
7. identify Strip 5;
8. fill every strip with a different low-brightness RGBW colour;
9. include at least one strip using only the dedicated White channel;
10. continue printing status information over USB serial.

Example final colours:

* Strip 1: low red;
* Strip 2: low green;
* Strip 3: low blue;
* Strip 4: low Neutral White;
* Strip 5: low combined RGBW colour.

## PIO architecture

The initial implementation shall use one PIO state machine for each physical
SK6812 RGBW strip.

Five PIO state machines shall control five independent GPIO outputs.

The state machines shall be distributed across the two RP2040 PIO blocks.

The driver shall support simultaneous transmission on all five outputs.

Each strip shall have an independent:

- pixel-buffer slice;
- pixel count;
- GPIO;
- DMA channel and transmission state;
- channel order;
- brightness setting.

Different strip lengths shall be supported.

The implementation shall not use a single packed parallel-output state machine
for the initial version.

Production transmission shall use one DMA channel for each enabled strip. Each
DMA channel shall feed its assigned PIO state machine TX FIFO and shall be
paced by that state machine's TX DREQ. CPU polling of PIO TX FIFOs is permitted
only in an isolated single-strip protocol-validation path; it shall not be used
for production frame transmission.

During initialization, the driver shall claim one currently unused DMA channel
for each usable strip and store the claimed channel number in that strip's
driver state. DMA channel identifiers shall not be hardcoded. No DMA claiming
or allocation shall occur during frame packing, transmission, or latching.
Claimed channel numbers and claim failures shall be reported through USB serial
diagnostics.

The design shall document:

- PIO block and state-machine allocation;
- how all five transmissions are started;
- how completion is detected;
- how reset/latch timing is enforced;
- how a shorter strip completes before a longer strip;
- whether DMA is used independently for every strip.

PIO0 SM0–SM3 shall be enabled in synchronization within PIO0. PIO1 SM0 shall
be started as part of the same concurrent frame operation; exact cycle-level
phase alignment between PIO0 and PIO1 is not required. A small bounded start
skew between the PIO blocks is acceptable and shall be documented.

DMA completion alone shall not be treated as physical completion. The PIO
program shall use a blocking pull after each transmitted 32-bit word. Before a
frame, the driver shall clear the relevant PIO TX-stall indication. After DMA
completion, the driver shall confirm that each enabled state machine has
stalled at its next blocking pull, proving that its final word has been shifted
out. TX FIFO empty alone is not sufficient proof. The latch timer shall start
only after the final enabled state machine has this confirmed completion.

## Frame scheduling

Every pixel requires 32 transmitted bits.

The design shall calculate the transmission duration based on:

* pixel count;
* 32 bits per pixel;
* nominal protocol bitrate;
* required reset interval.

When multiple strips are transmitted independently and simultaneously, the longest enabled strip determines the minimum complete frame-transmission duration.

When strips are transmitted sequentially, the sum of all strip lengths determines the duration.

The chosen design shall state whether transmission is:

* sequential;
* concurrent;
* partially concurrent.

The initial implementation shall use concurrent transmission. A frame shall
complete only after every enabled DMA transfer has completed, every enabled PIO
state machine has transmitted its final word, and the required reset/latch LOW
interval has elapsed.

## Pixel limits

The implementation shall define:

* maximum pixels per strip;
* maximum total configured pixels.

Approved initial values:

```text
Maximum pixels per strip: 300
Maximum configured pixels total: 1200
Default density: 60 pixels per metre
```

These values shall be easy to change in one configuration header.

The implementation shall reject:

* zero pixels for an enabled strip;
* more than the maximum pixels per strip;
* a total exceeding the maximum configured pixel count;
* unsupported physical channel order.

## Memory requirements

One logical RGBW pixel requires four bytes.

The design shall explicitly calculate:

* logical-buffer memory per strip;
* total logical-buffer memory;
* any output-conversion buffer;
* DMA buffer memory, if used;
* total static memory reserved for LED output.

The design shall clearly state whether buffers are:

* one buffer per strip;
* one shared pool;
* a fixed array with per-strip offsets;
* single-buffered;
* double-buffered.

The initial implementation shall use one shared logical RGBW pool and one
shared packed `uint32_t` DMA transmission pool, both with fixed per-strip
offsets. Both pools shall have capacity for 1,200 pixels. This is single
logical buffering; the DMA pool is an output-conversion buffer, not a second
logical pixel buffer.

`LedOutputManager` owns both static pools. Each `LedStrip` holds only
non-owning logical and packed-transmission slices assigned by the manager; it
does not own an independent pixel buffer.

No memory allocation shall occur during frame transmission.

The initial implementation shall avoid heap allocation in the normal rendering and transmission paths.

## Frame phases and buffer concurrency

Production frame handling shall use phases equivalent to `Idle`, `Packing`,
`Transmitting`, and `Latching`.

* In `Idle`, configuration, slice allocation, logical rendering, and a new
  frame start are permitted.
* In `Packing`, the logical pool is read to produce packed DMA words and shall
  not be modified concurrently. The packed DMA pool is written.
* In `Transmitting`, every active packed DMA slice is immutable until its DMA
  transfer completes. The logical pool is available for rendering the next
  frame after packing completes.
* In `Latching`, no new frame start, reconfiguration, or slice reallocation is
  permitted until the required LOW interval completes. The logical pool remains
  available for next-frame rendering.

Reconfiguration, slice reallocation, and a new frame start shall be rejected
with a typed busy status throughout `Packing`, `Transmitting`, and `Latching`.

## Pure-logic tests

The project shall provide a native host-side CMake test target that does not
require connected LED hardware or the Pico SDK runtime. It shall test RGBW and
GRBW packing, brightness scaling, stable logical indexing, reversed output
mapping, pixel-count validation, shared-pool slice allocation, non-overlapping
slices, and typed status behaviour.

## Direction reversal

Every strip shall support a logical reversed setting.

When a strip is reversed:

* logical pixel zero maps to the last physical pixel;
* effects do not need to know the physical installation direction;
* the electrical data direction does not change.

Direction reversal shall be implemented during logical-to-physical pixel mapping.

## Required project structure

The implementation shall use modules equivalent to:

```text
src/led/sk6812_rgbw_driver.cpp
src/led/led_strip.cpp
src/led/led_output_manager.cpp

include/led/sk6812_rgbw_driver.hpp
include/led/led_strip.hpp
include/led/led_output_manager.hpp
include/led/rgbw_color.hpp

pio/sk6812_rgbw.pio

tests/host/CMakeLists.txt
tests/host/led_logic_tests.cpp
```

Exact filenames may be adjusted in `design.md`.

The following responsibilities shall remain separate:

### SK6812 RGBW driver

Responsible for:

* PIO configuration;
* physical timing;
* 32-bit word transmission;
* latch/reset timing.

### LED strip

Responsible for:

* one strip configuration;
* non-owning logical and packed-transmission slices assigned by the manager;
* logical pixel access and stable logical indexes;
* brightness, physical channel order, and direction configuration.

### LED output manager

Responsible for:

* ownership of five strip instances;
* total-pixel validation;
* initialization of all outputs;
* asynchronous frame state and public transmission operations;
* sending all enabled strips;
* identification and hardware tests.

## Error reporting

The subsystem shall report explicit errors for:

* invalid strip index;
* invalid pixel index;
* invalid pixel count;
* excessive total pixel count;
* unsupported channel order;
* unavailable PIO state machine;
* unavailable DMA channel;
* failed strip initialization;
* attempted use of an uninitialized strip;
* attempted frame start or reconfiguration while a frame is active.

Errors shall be available through USB serial diagnostics.

A failure of one strip shall not silently corrupt the buffers of another strip.

Each strip shall retain its own initialization status. If at least one strip
fails while another configured strip remains usable, manager initialization
shall report a typed `partial_success` result and diagnostics shall name every
affected strip and its relevant GPIO, PIO, state-machine, and DMA resource.
Manager initialization results shall use these semantics:

* `ok`: all configured strips initialized;
* `partial_success`: at least one strip is usable and at least one failed;
* `failed`: no configured strip is usable.

## Excluded functionality

This feature shall not implement for now:

* audio capture;
* ADC DMA;
* FFT;
* music-reactive effects;
* web interface;
* Wi-Fi connection;
* persistent flash configuration;
* automatic RGB-to-White colour extraction;
* current estimation;
* automatic current limiting;
* firmware update.

## Definition of Done

The feature is complete when:

1. The project builds for Raspberry Pi Pico W.
2. Five independent SK6812 RGBW outputs are initialized.
3. Every transmitted pixel contains 32 bits.
4. Every strip can use a different pixel count.
5. Every strip can display independent Red, Green, Blue and White values.
6. The dedicated Neutral White channel is physically confirmed.
7. Channel order is configurable.
8. The user physically confirms the correct channel order.
9. Every strip can be identified independently.
10. Invalid pixel counts are rejected.
11. Excessive total pixel count is rejected.
12. No heap allocation occurs during frame transmission.
13. USB serial output continues to operate.
14. The design reports all LED-buffer RAM usage.
15. The test firmware uses limited brightness.
16. The user confirms the hardware test on all five outputs.
17. Production frame transmission uses PIO-paced DMA without CPU FIFO polling.
18. Frame completion is reported only after DMA completion, PIO drain, and the
    reset/latch interval.
