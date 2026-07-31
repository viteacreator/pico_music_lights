#pragma once

#include "audio/audio_processing.hpp"
#include "audio/spectrum_analyzer.hpp"
#include "led/led_status.hpp"

#include <cstdint>

struct DiagnosticRendererStats {
    uint32_t led_frames_started = 0;
    uint32_t led_frames_completed = 0;
    uint32_t led_frame_timeouts = 0;
    uint32_t led_frames_skipped_busy = 0;
    LedStatus led_last_status = LedStatus::ok;
};

// Initializes the fixed, temporary six-strip diagnostic scene. The module is
// internally disabled at initialization; audio_app enables it automatically
// after successful or partial LED initialization when a strip is usable.
bool diagnostic_renderer_initialize();
bool diagnostic_renderer_set_enabled(bool enabled);
bool diagnostic_renderer_is_enabled();
std::size_t diagnostic_renderer_usable_strip_count();
void diagnostic_renderer_service();
void diagnostic_renderer_update(const AudioLevelFrame& audio,
                                const SpectrumFrame& spectrum);
const DiagnosticRendererStats& diagnostic_renderer_stats();
void diagnostic_renderer_reset_stats();
