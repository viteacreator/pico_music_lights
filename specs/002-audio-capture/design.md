# Feature 002 — Tested Design

`audio_capture` owns ADC, ADC FIFO, one dynamically claimed ADC-DREQ DMA channel, IRQ, and two acquisition buffers. `audio_processing` is hardware-independent. `audio_app` schedules normal-loop processing and diagnostics. The VU renderer uses only the public LED manager API; audio DMA continues independently during LED DMA.

## Startup and ownership

Startup: (1) stop ADC; (2) drain FIFO; (3) clear sticky errors; (4) configure GP26–GP28 and ADC0→ADC1→ADC2 round robin; (5) configure DMA and IRQ; (6) start DMA; (7) start continuous ADC conversion.

Each buffer is `768 × 2 B = 1,536 B`; total static audio-buffer RAM is 3,072 B. States are Free, Filling, Ready, and Processing. DMA writes only Filling. IRQ marks a completed buffer Ready and starts the other only when Free. The main loop atomically marks Ready Processing, processes it, then releases Free. If no safe next buffer exists, the newly completed block is deliberately dropped, counted, and DMA restarts on that safe completed buffer. IRQ performs only handoff, counters, and restart.

## Processing

DC is Q8: initial block mean << 8, then one sixty-fourth of block-mean error. RMS is integer square root of mean square. Envelope attack shift is 1; release shift is 4. Raw ADC 0 and 4095 are low/high clipping. Mono is sample-wise after independent L/R centering and has its own peak/RMS/envelope. Runtime overflow and underflow are separately counted and cleared while preserving FIFO/DREQ configuration.
