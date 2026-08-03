#include "led/diagnostic_renderer.hpp"

#include <algorithm>
#include <array>
#include "board/led_board_config.hpp"
#include "effects/effect_scenes.hpp"
#include "led/diagnostic_rendering.hpp"
#include "led/led_output_manager.hpp"
#include "pico/time.h"

namespace {

constexpr std::array<uint16_t, board::kStripCount> kInstalledPixelCounts = {
    132,
    174,
    141,
    81,
    96,
    72,
};
constexpr uint8_t kSafeBrightness = 16;
constexpr uint64_t kRendererIntervalUs = 33334u;

static_assert(board::kStripCount == 6, "Expected six LED strips");
static_assert(board::kMaxConfiguredPixels == 800, "Unexpected LED pool size");

LedOutputManager g_manager;
effects::EffectEngine g_effect_engine;
DiagnosticRendererStats g_stats;
bool g_initialized = false;
bool g_enabled = false;
uint64_t g_last_update_us = 0;
uint64_t g_diagnostic_scene_started_us = 0;
effects::DiagnosticSceneId g_active_diagnostic_scene =
    effects::DiagnosticSceneId::gyver_vu_and_ambient;
bool g_diagnostic_scenes_enabled = true;
uint16_t g_volatile_gyver_left_noise_floor = 32u;
uint16_t g_volatile_gyver_right_noise_floor = 32u;

bool is_gyver_vu(const effects::StripEffectConfig& config) {
    return config.type == effects::EffectType::gyver_vu_gradient ||
           config.type == effects::EffectType::gyver_vu_rainbow;
}

void apply_volatile_gyver_vu_noise_floors(
    std::array<effects::StripEffectConfig, effects::kEffectStripCount>& scene) {
    for (effects::StripEffectConfig& config : scene) {
        if (is_gyver_vu(config)) {
            config.gyver_left_noise_floor = g_volatile_gyver_left_noise_floor;
            config.gyver_right_noise_floor = g_volatile_gyver_right_noise_floor;
        }
    }
}

const char* scene_name(effects::DiagnosticSceneId scene) {
    switch (scene) {
    case effects::DiagnosticSceneId::gyver_vu_and_ambient:
        return "gyver_vu_ambient";

    case effects::DiagnosticSceneId::gyver_frequency_and_spectrum:
        return "gyver_frequency_spectrum";

    case effects::DiagnosticSceneId::extended_generic:
        return "extended_generic";

    case effects::DiagnosticSceneId::extended_ambient_and_frequency:
        return "extended_ambient_frequency";
    }

    return "unknown";
}

void stage_due_diagnostic_scene(uint64_t now_us) {
    if (!g_diagnostic_scenes_enabled) {
        return;
    }

    const uint64_t duration_us = static_cast<uint64_t>(
        effects::diagnostic_scene_duration_ms(g_active_diagnostic_scene)) * 1000u;
    if (g_diagnostic_scene_started_us != 0u &&
        now_us - g_diagnostic_scene_started_us < duration_us) {
        return;
    }

    if (g_diagnostic_scene_started_us != 0u) {
        g_active_diagnostic_scene =
            effects::next_diagnostic_scene(g_active_diagnostic_scene);
    }

    std::array<effects::StripEffectConfig, effects::kEffectStripCount> scene =
        effects::diagnostic_scene(g_active_diagnostic_scene);
    apply_volatile_gyver_vu_noise_floors(scene);
    if (g_effect_engine.stage_scene(scene) ==
        effects::EffectStatus::ok) {
        g_diagnostic_scene_started_us = now_us;
    } else {
        ++g_stats.effect_frames_failed;
    }
}

LogicalRgbwPixels pixels_for_strip(std::size_t strip_index) {
    LedStrip* const strip = g_manager.strip(strip_index);

    if (strip == nullptr) {
        return {};
    }

    return {strip->logical_pixels(), strip->pixel_count()};
}

bool configure_manager() {
    std::array<LedStripConfig, board::kStripCount> configs{};

    for (std::size_t index = 0; index < configs.size(); ++index) {
        configs[index] = {
            true,
            kInstalledPixelCounts[index],
            board::kStripGpios[index],
            kSafeBrightness,
            ChannelOrder::grbw,
            false,
        };
    }

    if (g_manager.configure(configs) != LedStatus::ok) {
        return false;
    }

    const LedStatus initialization = g_manager.initialize_drivers();
    return initialization == LedStatus::ok ||
           initialization == LedStatus::partial_success;
}

}  // namespace

bool diagnostic_renderer_initialize() {
    if (g_initialized) {
        return true;
    }

    g_initialized = configure_manager();
    return g_initialized;
}

bool diagnostic_renderer_set_enabled(bool enabled) {
    if (!g_initialized) {
        return false;
    }

    g_enabled = enabled;
    return true;
}

bool diagnostic_renderer_is_enabled() {
    return g_enabled;
}

std::size_t diagnostic_renderer_usable_strip_count() {
    return g_initialized ? g_manager.usable_strip_count() : 0u;
}

void diagnostic_renderer_service() {
    if (!g_initialized || !g_manager.is_frame_in_progress()) {
        return;
    }

    const LedStatus status = g_manager.poll_frame_completion();

    if (status == LedStatus::busy) {
        return;
    }

    g_stats.led_last_status = status;
    if (status == LedStatus::ok) {
        ++g_stats.led_frames_completed;
        ++g_stats.effect_frames_completed;
    } else if (status == LedStatus::transmission_timeout) {
        ++g_stats.led_frame_timeouts;
    }
}

void diagnostic_renderer_update(const AudioLevelFrame& audio,
                                const SpectrumFrame& spectrum) {
    diagnostic_renderer_service();

    if (!g_initialized) {
        return;
    }

    const uint64_t now_us = time_us_64();
    const bool renderer_update_due =
        g_enabled && now_us - g_last_update_us >= kRendererIntervalUs;

    if (!renderer_update_due) {
        return;
    }

    if (g_manager.is_frame_in_progress()) {
        // One due diagnostic frame could not start. Advance the schedule so
        // normal-loop polls during the same busy interval do not inflate this
        // counter.
        g_last_update_us = now_us;
        ++g_stats.led_frames_skipped_busy;
        ++g_stats.effect_frames_skipped_busy;
        return;
    }

    if (!diagnostic_frame_should_start(
            g_enabled, false, now_us, g_last_update_us, kRendererIntervalUs)) {
        return;
    }

    std::array<effects::EffectRenderSpan, board::kStripCount> spans{};
    for (std::size_t index = 0; index < spans.size(); ++index) {
        const LogicalRgbwPixels pixels = pixels_for_strip(index);
        spans[index] = {pixels.data, pixels.size};
    }

    const uint64_t render_start_us = time_us_64();
    stage_due_diagnostic_scene(render_start_us);
    const effects::EffectInputSnapshot snapshot{
        &audio,
        &spectrum,
        render_start_us,
    };
    g_effect_engine.render(snapshot, spans);
    g_stats.effect_render_us = static_cast<uint32_t>(
        time_us_64() - render_start_us);
    g_stats.effect_render_max_us = std::max(
        g_stats.effect_render_max_us, g_stats.effect_render_us);

    const LedStatus status = g_manager.start_show_all_enabled();

    if (status == LedStatus::ok) {
        g_last_update_us = now_us;
        ++g_stats.led_frames_started;
        ++g_stats.effect_frames_started;
    } else if (status == LedStatus::busy) {
        g_last_update_us = now_us;
        ++g_stats.led_frames_skipped_busy;
        ++g_stats.effect_frames_skipped_busy;
    } else {
        g_stats.led_last_status = status;
        ++g_stats.effect_frames_failed;
    }
}

const DiagnosticRendererStats& diagnostic_renderer_stats() {
    return g_stats;
}

void diagnostic_renderer_reset_stats() {
    g_stats = {};
}

bool diagnostic_renderer_read_effect_config(
    std::size_t strip_index,
    effects::StripEffectConfig& output) {
    return g_effect_engine.read_config(strip_index, output);
}

effects::EffectStatus diagnostic_renderer_stage_effect_config(
    std::size_t strip_index,
    const effects::StripEffectConfig& config) {
    const effects::EffectStatus status =
        g_effect_engine.stage_strip_config(strip_index, config);
    if (status == effects::EffectStatus::ok) {
        g_diagnostic_scenes_enabled = false;
    }
    return status;
}

bool diagnostic_renderer_set_volatile_gyver_vu_noise_floors(
    uint16_t left_floor,
    uint16_t right_floor) {
    g_volatile_gyver_left_noise_floor = left_floor;
    g_volatile_gyver_right_noise_floor = right_floor;

    std::array<effects::StripEffectConfig, effects::kEffectStripCount> scene{};
    for (std::size_t index = 0u; index < scene.size(); ++index) {
        if (!g_effect_engine.read_config(index, scene[index])) {
            return false;
        }
    }
    apply_volatile_gyver_vu_noise_floors(scene);
    return g_effect_engine.stage_scene(scene) == effects::EffectStatus::ok;
}

bool diagnostic_renderer_volatile_gyver_vu_noise_floors(
    uint16_t& left_floor,
    uint16_t& right_floor) {
    left_floor = g_volatile_gyver_left_noise_floor;
    right_floor = g_volatile_gyver_right_noise_floor;
    return true;
}

bool diagnostic_renderer_read_effect_runtime(
    std::size_t strip_index,
    DiagnosticEffectRuntimeSnapshot& output) {
    const effects::StripEffectRuntime* const runtime =
        g_effect_engine.runtime(strip_index);
    if (runtime == nullptr) {
        return false;
    }

    output.config = runtime->config;
    output.left_reference = runtime->state.auto_gain_references[0u];
    output.right_reference = runtime->state.auto_gain_references[1u];
    output.left_gate_open = runtime->state.gyver_left_noise_gate_open;
    output.right_gate_open = runtime->state.gyver_right_noise_gate_open;
    return true;
}

effects::EffectStatus diagnostic_renderer_stage_effect_scene(
    const std::array<effects::StripEffectConfig, effects::kEffectStripCount>& scene) {
    const effects::EffectStatus status = g_effect_engine.stage_scene(scene);
    if (status == effects::EffectStatus::ok) {
        g_diagnostic_scenes_enabled = false;
    }
    return status;
}

bool diagnostic_renderer_restore_default_effect_scene() {
    g_effect_engine.restore_default_scene();
    g_diagnostic_scenes_enabled = false;
    return true;
}

uint32_t diagnostic_renderer_configuration_generation() {
    return g_effect_engine.configuration_generation();
}

const char* diagnostic_renderer_active_scene_name() {
    return g_diagnostic_scenes_enabled ? scene_name(g_active_diagnostic_scene) :
                                         "custom_or_reset";
}
