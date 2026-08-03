# Feature 004 - Physical Validation

Physical acceptance is pending.

1. Flash the Feature 004 UF2 to Pico W with all six SK6812 RGBW strips wired
   as the established GP2-GP7 GRBW configuration.
2. Confirm the startup report identifies the temporary diagnostic scene and a
   nonzero configuration generation. Every 12 seconds it shall cycle through
   Gyver VU/ambient, Gyver frequency/spectrum, generic spectrum/motion, and
   generic ambient/frequency demonstrations.
3. With quiet input for 60 seconds, confirm no growth in `adc_drop`,
   `missing_audio_blocks`, `dropped_windows`, `adc_over`, `adc_under`, or LED
   timeouts.
4. With ordinary music for 60 seconds, confirm all diagnostic views react and
   adapt to their own strip length. Verify a running-rainbow hue increment is
   constant from one logical pixel to the next. Stage a different effect only
   on Channel 1 and confirm Channels 2-6 remain unchanged.
5. Confirm Gyver Frequency 5 Zones reads High|Mid|Low|Mid|High and Gyver
   Frequency 3 Zones reads High|Mid|Low. Check reversal swaps the logical
   layouts, and verify Gyver Full Strip High-first and strongest-event policies.
6. Confirm Gyver Running Frequencies starts at the centre and moves matching
   colour history toward both ends on odd and even strips. Confirm the Gyver
   Spectrum Analyzer is mirrored with low bands at centre and high bands at
   ends.
7. With quiet input then changing music level, observe that Gyver event flashes
   decay and Gyver VU/Spectrum auto gain adapts. Set a nonzero RGBW background
   and verify it remains visible between events.
8. Confirm `effect_render_us` remains bounded and the 30 Hz renderer skips a
   due frame rather than delaying ready audio work.
9. Temporarily stage a White Boost static strip and check 0, 50, 100, 150, and
   200 percent: off; dedicated White about half; dedicated White; White plus
   half RGB assist; then White plus full RGB assist. Keep the configured global
   brightness at 16/255 and confirm power capacity before prolonged 200 percent
   operation.
10. Stage the named reset-default scene and confirm it yields six independent
    Gyver VU Gradient Green-Yellow-Orange-Red VUs. Confirm that this explicit
    action stops the temporary diagnostic sequence.
11. With quiet inputs run `vu_noise_calibrate 2000`, then use
    `vu_noise_floors`. Confirm floors are raw peak counts, quiet values leave
    Gyver VU bars off, and the bars decay normally after sound stops.
12. The strobe scene is rapid flashing: use appropriate photosensitivity
    precautions. Confirm Gyver Stroboscope switches directly between its full
    foreground and exact RGBW background with no intermediate fade.
