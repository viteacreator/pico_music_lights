# Feature 002 — Audio Capture Bring-Up

## Implemented behaviour

Feature 002 is implemented and physically tested on Raspberry Pi Pico W. Left is GP26/ADC0, Right is GP27/ADC1, and Aux is GP28/ADC2. ADC round-robin order is Left, Right, Aux. Aggregate rate is 96,000 samples/s; each channel is 32,000 samples/s. Each approximately 8 ms block contains 256 samples/channel, or 768 interleaved 16-bit samples.

Two static DMA acquisition buffers are used with no heap allocation. DMA starts before continuous ADC conversion. Inputs must be externally conditioned to 0–3.3 V, safely biased, and share reference ground.

Left, Right, and Aux have independent DC estimate, peak, RMS, envelope, and low/high clipping. Mono is sample-wise after independent DC removal: `(left_centered + right_centered) / 2`; it has independent peak, RMS, and envelope. Diagnostics report dropped blocks, FIFO overflow, FIFO underflow, and rate-limited USB status.

Temporary VU: Strip 1 Left, Strip 2 Right, Strip 3 Aux, Strip 4 Mono, Strips 5–6 off. LED updates are limited to approximately 60 frames/s.

## Definition of Done

Pico W firmware builds; ADC DMA continuously captures all inputs; USB diagnostics and VU operate; physical L, R, and Aux validation has passed. FFT, bands, beat detection, and effects are excluded.
