# Pico Music Lights — Project Context

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

The controller shall independently control five addressable LED strips.

The five strips may have different:

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

## Strip operating modes

Every strip shall independently support at least:

* Off
* Static colour
* Music reactive
* Test mode

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

## Development approach

The project uses Spec-Driven Development.

Every significant feature shall have:

* `requirements.md`;
* `design.md`;
* `tasks.md`.

The AI agent shall implement only explicitly approved tasks.

The project shall remain compilable after every completed task.
