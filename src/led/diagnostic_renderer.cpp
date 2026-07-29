#include "led/diagnostic_renderer.hpp"

#include <array>
#include <cstdio>

#include "board/led_board_config.hpp"
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
constexpr uint64_t kRendererIntervalUs = 16667u;

static_assert(board::kStripCount == 6, "Expected six LED strips");
static_assert(board::kMaxConfiguredPixels == 800, "Unexpected LED pool size");

LedOutputManager g_manager;
DiagnosticRendererStats g_stats;
bool g_initialized = false;
bool g_enabled = false;
uint64_t g_last_update_us = 0;

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
        std::printf("LED strip %u: GPIO %lu, pixels %u, GRBW, brightness %u\n",
                    static_cast<unsigned>(index + 1u),
                    static_cast<unsigned long>(configs[index].gpio),
                    static_cast<unsigned>(configs[index].pixel_count),
                    static_cast<unsigned>(configs[index].brightness));
    }

    if (g_manager.configure(configs) != LedStatus::ok) {
        return false;
    }

    const LedStatus initialization = g_manager.initialize_drivers();
    std::printf("LED initialization: %u (0=ok, 1=partial success, 2=failed)\n",
                static_cast<unsigned>(initialization));
    return initialization == LedStatus::ok ||
           initialization == LedStatus::partial_success;
}

void render_temporary_scene(const AudioLevelFrame& audio,
                            const SpectrumFrame& spectrum) {
    render_spectrum_32_to_16(pixels_for_strip(0), spectrum);
    render_symmetric_frequency_zones(pixels_for_strip(1), spectrum);
    render_bass_mid_high_zones(pixels_for_strip(2), spectrum);
    render_stereo_center_out(pixels_for_strip(3), audio.left, audio.right,
                             {255, 0, 0, 0}, {0, 0, 255, 0});
    render_full_vu(pixels_for_strip(4), audio.mono, {0, 255, 0, 0});
    render_full_vu(pixels_for_strip(5), audio.aux, {0, 160, 255, 32});
}

}  // namespace

bool diagnostic_renderer_initialize() {
    if (g_initialized) {
        return true;
    }

    std::printf("LED RAM: logical %u B, packed %u B\n",
                static_cast<unsigned>(board::kMaxConfiguredPixels * sizeof(RgbwColor)),
                static_cast<unsigned>(board::kMaxConfiguredPixels * sizeof(uint32_t)));
    g_initialized = configure_manager();
    return g_initialized;
}

void diagnostic_renderer_set_enabled(bool enabled) {
    g_enabled = enabled;
    std::printf("Diagnostic renderer %s\n", enabled ? "enabled" : "disabled");
}

bool diagnostic_renderer_is_enabled() {
    return g_enabled;
}

void diagnostic_renderer_service() {
    if (!g_initialized || !g_manager.is_frame_in_progress()) {
        return;
    }

    const LedStatus status = g_manager.poll_frame_completion();

    if (status != LedStatus::ok && status != LedStatus::busy) {
        std::printf("LED frame completion failed: status %u\n",
                    static_cast<unsigned>(status));
    }
}

void diagnostic_renderer_update(const AudioLevelFrame& audio,
                                const SpectrumFrame& spectrum) {
    diagnostic_renderer_service();

    if (!g_initialized) {
        return;
    }

    const uint64_t now_us = time_us_64();

    if (!diagnostic_frame_should_start(g_enabled, g_manager.is_frame_in_progress(),
                                       now_us, g_last_update_us,
                                       kRendererIntervalUs)) {
        if (g_initialized && g_enabled && g_manager.is_frame_in_progress()) {
            ++g_stats.skipped_busy_frames;
        }
        return;
    }

    render_temporary_scene(audio, spectrum);
    const LedStatus status = g_manager.start_show_all_enabled();

    if (status == LedStatus::ok) {
        g_last_update_us = now_us;
        ++g_stats.rendered_frames;
    } else if (status == LedStatus::busy) {
        ++g_stats.skipped_busy_frames;
    } else {
        std::printf("LED frame start failed: status %u\n",
                    static_cast<unsigned>(status));
    }
}

const DiagnosticRendererStats& diagnostic_renderer_stats() {
    return g_stats;
}

void diagnostic_renderer_reset_stats() {
    g_stats = {};
}
