# Feature 002 — Design

One dynamically claimed DMA channel is paced by ADC DREQ. ADC round-robin ADC0–ADC2 writes 16-bit FIFO samples into two 1,536-byte static buffers. DMA IRQ only marks a completed buffer, counts overwrite, and starts DMA on the other buffer. It performs neither processing nor logging.

Pure `audio_processing` deinterleaves L/R/Aux and uses integer arithmetic. A per-channel Q8 DC estimator initializes from the first block mean and then updates by one sixty-fourth of mean error per block. Peak is absolute centered amplitude; RMS is integer square root of mean square; envelope uses faster attack than release. `AudioLevelFrame` has no ADC or LED dependency.

The main loop services ready blocks, renders VU frames only when LED frame state is idle, polls LED completion, and rate-limits USB diagnostics. No heap allocation or long delay is used.
