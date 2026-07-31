# Feature 004 — Physical Validation

Physical acceptance is pending.

1. Flash the Feature 004 UF2 to Pico W with all six SK6812 RGBW strips wired
   as the established GP2–GP7 GRBW configuration.
2. Confirm the startup report lists six independent default effects and a
   nonzero effect configuration generation.
3. With quiet input for 60 seconds, confirm no growth in `adc_drop`,
   `missing_audio_blocks`, `dropped_windows`, `adc_over`, `adc_under`, or LED
   timeouts.
4. With ordinary music for 60 seconds, confirm all six views react
   independently: spectrum, zones, macro colours, stereo centre-out, Mono VU,
   and Aux VU. Confirm no strip corrupts another span.
5. Confirm `effect_render_us` remains bounded and the 30 Hz renderer skips a
   due frame rather than delaying ready audio work.
6. Temporarily stage a White Boost static strip and check 0, 50, 100, 150, and
   200 percent: off; dedicated White about half; dedicated White; White plus
   half RGB assist; then White plus full RGB assist. Keep the configured global
   brightness at 16/255 and confirm power capacity before prolonged 200 percent
   operation.
