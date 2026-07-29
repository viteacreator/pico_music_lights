#pragma once

#include "audio/audio_processing.hpp"
#include "audio/spectrum_analyzer.hpp"

#include <cstdint>

struct DiagnosticRendererStats {
    uint32_t rendered_frames = 0;
    uint32_t skipped_busy_frames = 0;
};

// Initializes the fixed, temporary six-strip diagnostic scene. The renderer
// is disabled by default so spectrum validation can run without LED updates.
bool diagnostic_renderer_initialize();
bool diagnostic_renderer_set_enabled(bool enabled);
bool diagnostic_renderer_is_enabled();
void diagnostic_renderer_service();
void diagnostic_renderer_update(const AudioLevelFrame& audio,
                                const SpectrumFrame& spectrum);
const DiagnosticRendererStats& diagnostic_renderer_stats();
void diagnostic_renderer_reset_stats();
