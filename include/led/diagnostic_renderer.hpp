#pragma once

#include "audio/audio_processing.hpp"
#include "audio/spectrum_analyzer.hpp"
#include "effects/effect_engine.hpp"
#include "led/led_status.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

struct DiagnosticRendererStats {
    uint32_t led_frames_started = 0;
    uint32_t led_frames_completed = 0;
    uint32_t led_frame_timeouts = 0;
    uint32_t led_frames_skipped_busy = 0;
    LedStatus led_last_status = LedStatus::ok;
    uint32_t effect_frames_started = 0;
    uint32_t effect_frames_completed = 0;
    uint32_t effect_frames_skipped_busy = 0;
    uint32_t effect_render_us = 0;
    uint32_t effect_render_max_us = 0;
};

// Initializes the LED transport wrapper and its compiled Effect Engine scene.
// The runtime is internally disabled at initialization; audio_app enables it
// automatically after successful or partial LED initialization when a strip is
// usable.
bool diagnostic_renderer_initialize();
bool diagnostic_renderer_set_enabled(bool enabled);
bool diagnostic_renderer_is_enabled();
std::size_t diagnostic_renderer_usable_strip_count();
void diagnostic_renderer_service();
void diagnostic_renderer_update(const AudioLevelFrame& audio,
                                const SpectrumFrame& spectrum);
const DiagnosticRendererStats& diagnostic_renderer_stats();
void diagnostic_renderer_reset_stats();
bool diagnostic_renderer_read_effect_config(
    std::size_t strip_index,
    effects::StripEffectConfig& output);
effects::EffectStatus diagnostic_renderer_stage_effect_config(
    std::size_t strip_index,
    const effects::StripEffectConfig& config);
effects::EffectStatus diagnostic_renderer_stage_effect_scene(
    const std::array<effects::StripEffectConfig, effects::kEffectStripCount>& scene);
bool diagnostic_renderer_restore_default_effect_scene();
uint32_t diagnostic_renderer_configuration_generation();
