# Feature 002 — Audio Capture Bring-Up

Capture Left (GP26/ADC0), Right (GP27/ADC1), and Aux (GP28/ADC2) continuously on Raspberry Pi Pico W using ADC FIFO and DMA. The aggregate rate is 96,000 samples/s, yielding 32,000 samples/s per channel in fixed L/R/Aux order.

DMA uses two static ping-pong buffers of 768 16-bit samples (256 per channel). Processing provides DC offset, peak, RMS/envelope, clipping and derived L+R levels. USB diagnostics are rate limited. A temporary VU display maps L, R, Aux and L+R to strips 1–4; strips 5–6 are off.

Inputs must be externally conditioned to 0–3.3 V and biased near ADC midpoint. Never connect speaker-level or negative-voltage audio directly; share reference ground with the source. FFT, bands, beat detection, AGC and final effects are excluded.
