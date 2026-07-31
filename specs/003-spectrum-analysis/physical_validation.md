# Feature 003.1 — Physical Validation Procedure

Physical acceptance remains pending. Perform both a 60-second quiet-input run
and a separate 60-second ordinary-music run after startup; counters are judged
from the first stable telemetry line, not reset or hidden.

The supplied preliminary physical run observed `fft_max_us=12295`,
`audio_work_max_us=12728`, zero audio continuity losses, and zero LED timeouts.
The two extended runs below remain the final confirmation.

1. Flash the UF2 and open USB CDC at any time after boot. The two retained
   `DBG startup` lines must appear once after the terminal connects. Confirm
   `audio=ok`, expected strip count, `backend=q15`, and automatic renderer state.
2. With quiet conditioned inputs, copy at least five one-second `DBG t_ms=`
   lines. Preferred `fft_us` and `audio_work_us` are below 8,000 us; the
   practical hard limit is comfortably below approximately 16,000 us. Final
   acceptance requires no growth in `adc_drop`, `missing_audio_blocks`,
   `dropped_windows`, `adc_over`, or `adc_under`. The raw fields are
   normalized-power milli-units:
   `raw_mean_milli` and `raw_max_milli`.
3. Apply conditioned 80 Hz, 300 Hz, 1 kHz, and 6 kHz tones. Copy three lines
   per tone. Verify Bass, Low, Mid, and High respectively dominate.
4. When practical, apply the approximate 313-centred-count reference tone and
   copy its lines. Also verify that a quiet input remains dark; do not change
   floor, gain, reference energy, smoothing, or visual normalization merely to
   compensate for low-level noise. Then test ordinary music.
5. Verify automatic temporary rendering: Strip 1 spectrum, Strip 2 symmetric
   zones, Strip 3 Bass/Mid/High, Strip 4 Left/Right, Strip 5 Mono, Strip 6 Aux.
   Confirm moderate input is visible and quiet input is mostly dark.
6. Leave the terminal connected but idle for several seconds. Copy telemetry
   and report whether any continuity counter changes. Confirm
   `led_frames_started` and `led_frames_completed` increase together and
   `led_frame_timeouts` does not increase.

Return the startup block and copied `DBG` lines for review. Do not tune floor,
gain, reference energy, or visual-normalization constants without these
measurements.
