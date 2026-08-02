#include <algorithm>
#include <array>
#include <cstdio>

#include "audio/audio_capture.hpp"
#include "audio/spectrum_analyzer.hpp"
#include "led/diagnostic_renderer.hpp"
#include "led/diagnostic_rendering.hpp"
#include "pico/stdlib.h"
#include "tusb.h"

namespace {

constexpr uint64_t kDiagnosticsPeriodUs = 1'000'000u;
constexpr uint32_t kAudioAggregateSampleRate = 96'000u;
constexpr uint32_t kMonoSampleRate = 32'000u;
constexpr uint32_t kSpectrumOverlapPercent = 50u;
constexpr std::size_t kTelemetryBufferSize = 512u;
// The Feature 004 default-scene line makes the deferred startup report larger
// than the previous 320-byte buffer. Keep it below the bounded 512-byte USB
// CDC transmit buffer so startup acknowledgement cannot suppress telemetry.
constexpr std::size_t kStartupBufferSize = 512u;

AudioLevelFrame g_audio_frame{};
CenteredMonoBlock g_centered_mono{};
SpectrumAnalyzer g_spectrum_analyzer;
SpectrumFrame g_spectrum_frame{};
std::array<char, kTelemetryBufferSize> g_telemetry_line{};
std::array<char, kStartupBufferSize> g_startup_report{};
std::size_t g_startup_report_length = 0;
uint32_t g_maximum_analysis_us = 0;
uint32_t g_audio_work_us = 0;
uint32_t g_maximum_audio_work_us = 0;
bool g_renderer_available = false;
bool g_startup_report_delivered = false;

void prepare_startup_report() {
    const int length = std::snprintf(
        g_startup_report.data(),
        g_startup_report.size(),
        "DBG startup build=%s audio=ok renderer=%s usable_strips=%u "
        "aggregate_hz=%lu mono_hz=%lu fft_size=%u overlap_pct=%lu backend=q15 "
        "noise_floor=1 gain=1 reference_energy=32768 renderer_enabled=%s\n"
        "DBG pixels=132,174,141,81,96,72 order=GRBW brightness=16\n"
        "DBG reset_default=6x_stereo_vu_gradient diagnostic_scene=%s cycle_s=12 generation=%lu\n",
        PICO_PROGRAM_VERSION_STRING,
        g_renderer_available ? "ok" : "unavailable",
        static_cast<unsigned>(diagnostic_renderer_usable_strip_count()),
        static_cast<unsigned long>(kAudioAggregateSampleRate),
        static_cast<unsigned long>(kMonoSampleRate),
        static_cast<unsigned>(kSpectrumWindowSamples),
        static_cast<unsigned long>(kSpectrumOverlapPercent),
        diagnostic_renderer_is_enabled() ? "yes" : "no",
        diagnostic_renderer_active_scene_name(),
        static_cast<unsigned long>(
            diagnostic_renderer_configuration_generation()));

    if (length > 0 &&
        static_cast<std::size_t>(length) < g_startup_report.size()) {
        g_startup_report_length = static_cast<std::size_t>(length);
    }
}

bool try_deliver_startup_report() {
    if (g_startup_report_delivered) {
        return true;
    }

    if (g_startup_report_length == 0u ||
        !tud_cdc_connected() ||
        tud_cdc_write_available() < g_startup_report_length) {
        return false;
    }

    (void)tud_cdc_write(g_startup_report.data(), g_startup_report_length);
    tud_cdc_write_flush();
    g_startup_report_delivered = true;
    return true;
}

void print_telemetry() {
    const DiagnosticRendererStats& renderer = diagnostic_renderer_stats();
    const int length = std::snprintf(
        g_telemetry_line.data(),
        g_telemetry_line.size(),
        "DBG audio_seq=%lu L=%u R=%u Aux=%u Mono=%u adc_drop=%lu "
        "adc_over=%lu adc_under=%lu spectrum_seq=%lu bass=%u low=%u mid=%u high=%u "
        "fft_us=%lu fft_max_us=%lu dropped_windows=%lu missing_audio_blocks=%lu "
        "audio_work_us=%lu audio_work_max_us=%lu scene=%s cfg_gen=%lu "
        "effect_us=%lu effect_max_us=%lu effect_skip=%lu effect_fail=%lu "
        "led_frame_timeouts=%lu\n",
        static_cast<unsigned long>(g_audio_frame.sequence),
        g_audio_frame.left,
        g_audio_frame.right,
        g_audio_frame.aux,
        g_audio_frame.mono,
        static_cast<unsigned long>(g_audio_frame.dropped_blocks),
        static_cast<unsigned long>(audio_capture_fifo_errors()),
        static_cast<unsigned long>(audio_capture_fifo_underflows()),
        static_cast<unsigned long>(g_spectrum_frame.sequence),
        g_spectrum_frame.bass,
        g_spectrum_frame.low,
        g_spectrum_frame.mid,
        g_spectrum_frame.high,
        static_cast<unsigned long>(g_spectrum_frame.analysis_time_us),
        static_cast<unsigned long>(g_maximum_analysis_us),
        static_cast<unsigned long>(g_spectrum_frame.dropped_windows),
        static_cast<unsigned long>(g_spectrum_frame.missing_audio_blocks),
        static_cast<unsigned long>(g_audio_work_us),
        static_cast<unsigned long>(g_maximum_audio_work_us),
        diagnostic_renderer_active_scene_name(),
        static_cast<unsigned long>(diagnostic_renderer_configuration_generation()),
        static_cast<unsigned long>(renderer.effect_render_us),
        static_cast<unsigned long>(renderer.effect_render_max_us),
        static_cast<unsigned long>(renderer.effect_frames_skipped_busy),
        static_cast<unsigned long>(renderer.effect_frames_failed),
        static_cast<unsigned long>(renderer.led_frame_timeouts));

    if (length <= 0 ||
        static_cast<std::size_t>(length) >= g_telemetry_line.size() ||
        !tud_cdc_connected() ||
        tud_cdc_write_available() < static_cast<uint32_t>(length)) {
        return;
    }

    (void)tud_cdc_write(g_telemetry_line.data(), static_cast<uint32_t>(length));
    tud_cdc_write_flush();
}

}  // namespace

int main() {
    stdio_init_all();

    g_renderer_available = diagnostic_renderer_initialize();
    if (diagnostic_renderer_should_auto_enable(
            g_renderer_available, diagnostic_renderer_usable_strip_count())) {
        (void)diagnostic_renderer_set_enabled(true);
    }

    if (!audio_capture_initialize()) {
        std::printf("DBG fatal audio_initialization=failed\n");
        while (true) {
            tight_loop_contents();
        }
    }

    prepare_startup_report();

    uint64_t last_telemetry_us = 0;
    while (true) {
        diagnostic_renderer_service();

        const uint64_t audio_work_start_us = time_us_64();
        if (audio_capture_process(g_audio_frame, g_centered_mono)) {
            const uint64_t analysis_start_us = time_us_64();
            const bool new_spectrum =
                g_spectrum_analyzer.push(g_centered_mono, g_spectrum_frame);

            if (new_spectrum) {
                g_spectrum_frame.analysis_time_us =
                    static_cast<uint32_t>(time_us_64() - analysis_start_us);
                g_maximum_analysis_us = std::max(
                    g_maximum_analysis_us, g_spectrum_frame.analysis_time_us);
                g_spectrum_frame.maximum_analysis_time_us = g_maximum_analysis_us;
            }

            g_audio_work_us = static_cast<uint32_t>(
                time_us_64() - audio_work_start_us);
            g_maximum_audio_work_us = std::max(
                g_maximum_audio_work_us, g_audio_work_us);

            // Every processed audio block yields directly to the next capture
            // opportunity. Rendering and telemetry run only in idle-audio
            // iterations below.
            continue;
        }

        // The capture IRQ may have completed a block after the failed acquire.
        // Do not begin optional work when that happens.
        if (audio_capture_has_ready_block()) {
            continue;
        }

        if (g_renderer_available) {
            diagnostic_renderer_update(g_audio_frame, g_spectrum_frame);
        }

        if (audio_capture_has_ready_block()) {
            continue;
        }

        const uint64_t now_us = time_us_64();
        if (!try_deliver_startup_report()) {
            tight_loop_contents();
            continue;
        }

        if (last_telemetry_us == 0u) {
            last_telemetry_us = now_us;
        } else if (now_us - last_telemetry_us >= kDiagnosticsPeriodUs) {
            last_telemetry_us = now_us;
            print_telemetry();
        }

        tight_loop_contents();
    }
}
