# Feature 003 — Physical Validation Procedure

1. Flash the Feature 003 UF2 and open USB serial at the configured USB CDC
   connection. Confirm the banner says renderer default off.
2. Leave audio inputs quiet and send `status`. Then send `noise measure` and
   copy the final `Noise measurement:` line plus one preceding periodic
   `audio #...` line.
3. From quiet input, copy the highest reported `fft` and `max` values. Confirm
   `adc_drop`, `over`, `under`, `windows`, and `missing` remain zero.
4. Apply conditioned test tones at 80 Hz, 300 Hz, 1 kHz, then 6 kHz. For each,
   copy one periodic line. Expected dominant macro order is Bass, Low, Mid,
   High respectively; exact amplitudes are not yet acceptance tuning.
5. When practical, apply a bin-centred/reference-like signal near 313 centered
   ADC counts and copy its diagnostic line. State how its level was estimated.
6. Play ordinary music and copy several periodic lines, especially any counter
   changes or timing maximum.
7. Send `renderer on`. Verify strip 1 spectrum direction, strip 2 mirrored
   zones, strip 3 Bass/Mid/High, strip 4 centre-out Left/Right, strip 5 Mono,
   and strip 6 Aux. Send `renderer off` and verify no new frames are requested.
8. Send final `status` and copy it back with all tone/noise lines. Report any
   nonzero drop, missing, overflow, or underflow value.

Do not adjust provisional floor, gain, reference energy, attack, or release
until these measurements are reviewed.
