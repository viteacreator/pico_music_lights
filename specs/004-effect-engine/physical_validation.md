# Feature 004 — Physical Validation

Physical acceptance is pending.

1. Flash the Feature 004 UF2 to Pico W with all six SK6812 RGBW strips wired
   as the established GP2–GP7 GRBW configuration.
2. Confirm the startup report identifies the temporary diagnostic scene and a
   nonzero effect configuration generation. Every 12 seconds the scene shall
   atomically alternate between VU/ambient and spectrum/motion demonstrations;
   together they exercise the supported catalog across all six strips.
3. With quiet input for 60 seconds, confirm no growth in `adc_drop`,
   `missing_audio_blocks`, `dropped_windows`, `adc_over`, `adc_under`, or LED
   timeouts.
4. With ordinary music for 60 seconds, confirm all diagnostic views react and
   adapt to their own strip length. Verify a running-rainbow hue increment is
   constant from one logical pixel to the next. Stage a different effect only
   on Channel 1 and confirm Channels 2–6 remain unchanged.
5. Confirm `effect_render_us` remains bounded and the 30 Hz renderer skips a
   due frame rather than delaying ready audio work.
6. Temporarily stage a White Boost static strip and check 0, 50, 100, 150, and
   200 percent: off; dedicated White about half; dedicated White; White plus
   half RGB assist; then White plus full RGB assist. Keep the configured global
   brightness at 16/255 and confirm power capacity before prolonged 200 percent
   operation.
7. Stage the named reset-default scene and confirm it yields six independent
   Stereo Centre-Out Green→Yellow→Orange→Red VUs. Confirm that this explicit
   action stops the temporary diagnostic sequence.
