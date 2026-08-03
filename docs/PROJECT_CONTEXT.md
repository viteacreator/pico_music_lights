# Pico Music Lights â€” Project Context

## Project goal

Create a real-time music-reactive addressable LED controller using Raspberry Pi Pico W.

The project is inspired by AlexGyver ColorMusic for ATmega328P, but it shall not be implemented as a direct port.

The original project is used only as a functional and visual reference.

The new implementation shall use the additional processing resources and hardware capabilities of the RP2040.

## Target hardware

* Board: Raspberry Pi Pico W
* MCU: RP2040
* Language: C++17
* Framework: Raspberry Pi Pico SDK
* Build system: CMake
* Development environment: Visual Studio Code
* Operating system used for development: Windows

## Audio inputs

The controller shall use three analogue audio inputs:

* Left;
* Right;
* Aux.

The Aux input may be connected to a microphone or another external analogue
audio source.

Logical audio sources available to effects shall include:

* Left;
* Right;
* Aux;
* Left plus Right;
* derived Bass;
* derived Low;
* derived Mid;
* derived High.

Bass, Low, Mid, and High shall be derived from the Left and Right inputs.
They shall not require a dedicated physical Bass input.

Future sources may include:

* stereo maximum;
* stereo average;
* frequency bands produced by FFT analysis;
* other derived audio features.

## LED outputs

The controller shall independently control six addressable LED strips.

The six strips may have different:

* physical lengths;
* pixel counts;
* operating modes;
* audio sources;
* visual effects;
* brightness levels;
* effect parameters.

The initial LED hardware shall use SK6812 RGBW addressable LED strips.

Strip properties:

- SK6812 RGBW;
- SMD5050 package;
- 60 individually addressable pixels per metre;
- four independent channels per pixel: Red, Green, Blue and White;
- dedicated Neutral White channel;
- approximately 800 kbit/s single-wire protocol;
- 32 transmitted bits per pixel.

The physical channel transmission order shall be configurable and verified through a hardware test. It shall not be assumed solely from the product listing.

Initial installed strip layout:

| Strip | GPIO | Physical length | Configured pixels |
| --- | --- | ---: | ---: |
| 1 | GP2 | 2.20 m | 132 |
| 2 | GP3 | 2.90 m | 174 |
| 3 | GP4 | 2.35 m | 141 |
| 4 | GP5 | 1.35 m | 81 |
| 5 | GP6 | 1.60 m | 96 |
| 6 | GP7 | 1.20 m | 72 |

The installed total is 696 pixels. The firmware reserves capacity for at most
800 configured pixels total, with a separate maximum of 300 pixels per strip.

## Strip operating modes

Every strip shall independently support at least:

* Off
* Static Direct RGBW colour
* Static White Boost colour
* Music reactive
* Test mode

White Boost is a logical RGBW drive mode. It drives only the dedicated White
channel from 0 to 100 percent, then holds White at maximum while blending a
configurable RGB assist colour from 100 to 200 percent. It is not a calibrated
claim of twice the luminous output: it is applied before strip brightness and
future current limiting, and may require substantially more electrical current
than dedicated White alone.

Future modes may include:

* ambient animation;
* follow another strip;
* scenes;
* automatic effect rotation.

## Web configuration

The Pico W shall provide a local web interface.

The web interface shall configure:

* pixel count for each strip;
* physical length and LED density;
* strip operating mode;
* selected audio source;
* selected effect;
* brightness;
* effect parameters;
* audio sensitivity;
* Wi-Fi settings;
* system settings.

The web interface shall operate without an internet connection.

## Wi-Fi behaviour

The device shall support:

* connection to an existing Wi-Fi network;
* local access-point mode;
* configuration through a local webpage.

If the device cannot connect to a saved network, it shall provide a recovery access point.

A physical CONFIG button shall be able to force access-point mode.

## Firmware update

The initial firmware shall be updated through USB using a UF2 file.

Web-based firmware update may be implemented later.

Because the target is RP2040 with 2 MB flash, OTA architecture shall not be implemented until the normal firmware size and flash usage are known.

## Architectural principles

The following subsystems shall remain separate:

* audio capture;
* audio analysis;
* visual effects;
* LED transmission;
* configuration;
* Wi-Fi;
* web server;
* persistent storage;
* system monitoring.

Effects shall not directly access:

* ADC hardware;
* GPIO registers;
* Wi-Fi;
* flash storage;
* the web server.

Effects shall receive processed audio data and render into pixel buffers.

Feature 004 provides a hardware-independent Effect Engine with six fixed,
independent strip runtimes. Every runtime owns its configuration and temporal
state; effects receive an explicit logical pixel span and one shared read-only
audio/spectrum snapshot. A strip may independently select effect, compatible
source, RGBW colours, geometry, direction and response parameters. The engine
performs one shared audio analysis/FFT cycle, not one analysis per strip.

Effect configuration is validated and applied atomically at a render boundary.
The normal renderer skips a due frame if LED transport is busy, rather than
compromising audio continuity. Wi-Fi, web configuration and persistence will
use the engine's application-facing configuration API in later features.

The named future reset-default scene deliberately contains six value-copied, independent
Gyver VU Gradient configurations. Each uses the shared Left/Right source,
Off background, and a logical centre-to-end Green â†’ Yellow â†’ Orange â†’ Red
gradient. This is a default scene only: no physical strip is a master and every
strip may later select any compatible effect and parameters independently.

Feature 004 physical bring-up uses a separate, temporary, atomic diagnostic
scene sequence rather than this reset scene. The sequence alternates timed
VU/ambient and spectrum/motion scenes across the six strips, and can be
disabled by explicitly staging a custom scene or restoring the reset-default
scene.

The reusable Feature 004 catalog covers static RGBW/White Boost, solid,
gradient and rainbow VU modes, mirrored/macro/one-band/spectrum frequency
views, stroboscope, smooth colour cycle, running rainbow, and running
frequencies. These are inspired by the user-visible AlexGyver ColorMusic modes,
not a port of its Arduino hardware implementation.

Feature 004 also keeps a separate Gyver-compatible catalog (referencing AlexGyver ColorMusic):
stereo gradient/rainbow VU, adaptive 5-zone/3-zone/full-strip/running-frequency
effects, mirrored spectrum, and RGBW strobe/ambient variants. These consume the
same shared audio and spectrum frames once per cycle. Their adaptive detector,
auto gain, background, colour history and configuration remain independent for
each strip. The generic circular trail remains available under the explicit
name `frequency_comet`.

The internal pixel representation shall preserve all four physical channels.

Effects shall render RGBW values, not RGB values with an artificial alpha channel.

The White channel shall be independently controllable and shall not be discarded during colour conversion, brightness limiting or transmission.

## Resource constraints

The project targets Pico W, not Pico 2 W.

Therefore:

* SRAM usage must be monitored;
* flash usage must be monitored;
* dynamic allocation in real-time processing should be avoided;
* web resources must remain compact;
* pixel buffers shall have explicit compile-time limits;
* FFT size shall remain moderate;
* networking shall not interrupt audio processing;
* LED transmission shall use PIO and DMA where practical, with deterministic
  high-rate work offloaded from the CPU when resource complexity is justified.

Each logical pixel requires at least four bytes of pixel-buffer memory:

- one byte for Red;
- one byte for Green;
- one byte for Blue;
- one byte for White.

## Global Idle Lighting

Feature 004 provides a global, optional Idle Lighting overlay. It is separate
from all six per-strip effect configurations: effects continue rendering while
the overlay blends their RGBW pixels toward a configurable idle colour. Audio
activity is detected directly from raw Left, Right and Aux peaks using global
input selection, floors, hysteresis, confirmation and silence timing. The
default idle colour uses the dedicated SK6812 White channel. Idle configuration
is staged and applied at a renderer frame boundary, ready for a future web UI;
it is not persistent in this feature.

## Development approach

The project uses Spec-Driven Development.

Every significant feature shall have:

* `requirements.md`;
* `design.md`;
* `tasks.md`.

The AI agent shall implement only explicitly approved tasks.

The project shall remain compilable after every completed task.
