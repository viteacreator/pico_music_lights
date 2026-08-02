#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstdio>
#include <cstring>

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
constexpr std::size_t kCommandBufferSize = 64u;
constexpr uint32_t kDefaultVuCalibrationDurationMs = 2000u;
constexpr uint32_t kMinimumVuCalibrationDurationMs = 250u;
constexpr uint32_t kMaximumVuCalibrationDurationMs = 10000u;
constexpr uint16_t kVuCalibrationSafetyMargin = 8u;
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
std::array<char, kCommandBufferSize> g_command_line{};
std::size_t g_command_length = 0u;
bool g_command_discard_until_newline = false;
bool g_vu_calibration_active = false;
uint64_t g_vu_calibration_deadline_us = 0u;
uint16_t g_vu_calibration_left_peak = 0u;
uint16_t g_vu_calibration_right_peak = 0u;
uint16_t g_vu_calibration_last_left_floor = 0u;
uint16_t g_vu_calibration_last_right_floor = 0u;

bool write_usb_line(const char* line, std::size_t length) {
    if (line == nullptr || length == 0u || !tud_cdc_connected() ||
        tud_cdc_write_available() < length) {
        return false;
    }

    (void)tud_cdc_write(line, static_cast<uint32_t>(length));
    tud_cdc_write_flush();
    return true;
}

void print_command_reply(const char* message) {
    const int length = std::snprintf(g_telemetry_line.data(),
                                     g_telemetry_line.size(),
                                     "DBG command %s\n",
                                     message);
    if (length > 0 && static_cast<std::size_t>(length) < g_telemetry_line.size()) {
        (void)write_usb_line(g_telemetry_line.data(), static_cast<std::size_t>(length));
    }
}

uint16_t calibrated_floor(uint16_t peak) {
    return static_cast<uint16_t>(std::min<uint32_t>(
        2047u, static_cast<uint32_t>(peak) + kVuCalibrationSafetyMargin));
}

void print_vu_floors() {
    DiagnosticEffectRuntimeSnapshot runtime{};
    if (!g_renderer_available ||
        !diagnostic_renderer_read_effect_runtime(0u, runtime)) {
        print_command_reply("vu_noise_floors renderer=unavailable");
        return;
    }

    const int length = std::snprintf(
        g_telemetry_line.data(),
        g_telemetry_line.size(),
        "DBG command vu_noise_floors left=%u right=%u hysteresis=%u last_left=%u last_right=%u\n",
        runtime.config.gyver_left_noise_floor,
        runtime.config.gyver_right_noise_floor,
        runtime.config.gyver_noise_gate_hysteresis,
        g_vu_calibration_last_left_floor,
        g_vu_calibration_last_right_floor);
    if (length > 0 && static_cast<std::size_t>(length) < g_telemetry_line.size()) {
        (void)write_usb_line(g_telemetry_line.data(), static_cast<std::size_t>(length));
    }
}

void begin_vu_calibration(uint32_t duration_ms) {
    if (!g_renderer_available) {
        print_command_reply("vu_noise_calibrate renderer=unavailable");
        return;
    }

    g_vu_calibration_active = true;
    g_vu_calibration_left_peak = 0u;
    g_vu_calibration_right_peak = 0u;
    g_vu_calibration_deadline_us =
        time_us_64() + static_cast<uint64_t>(duration_ms) * 1000u;
    const int length = std::snprintf(g_telemetry_line.data(),
                                     g_telemetry_line.size(),
                                     "DBG command vu_noise_calibrate started duration_ms=%lu\n",
                                     static_cast<unsigned long>(duration_ms));
    if (length > 0 && static_cast<std::size_t>(length) < g_telemetry_line.size()) {
        (void)write_usb_line(g_telemetry_line.data(), static_cast<std::size_t>(length));
    }
}

void apply_vu_calibration() {
    const uint16_t left_floor = calibrated_floor(g_vu_calibration_left_peak);
    const uint16_t right_floor = calibrated_floor(g_vu_calibration_right_peak);
    bool applied = false;

    for (std::size_t index = 0u; index < effects::kEffectStripCount; ++index) {
        effects::StripEffectConfig config{};
        if (!diagnostic_renderer_read_effect_config(index, config) ||
            (config.type != effects::EffectType::gyver_vu_gradient &&
             config.type != effects::EffectType::gyver_vu_rainbow)) {
            continue;
        }

        config.gyver_left_noise_floor = left_floor;
        config.gyver_right_noise_floor = right_floor;
        if (diagnostic_renderer_stage_effect_config(index, config) !=
            effects::EffectStatus::ok) {
            print_command_reply("vu_noise_calibrate apply=failed");
            return;
        }
        applied = true;
    }

    g_vu_calibration_last_left_floor = left_floor;
    g_vu_calibration_last_right_floor = right_floor;
    const int length = std::snprintf(
        g_telemetry_line.data(),
        g_telemetry_line.size(),
        "DBG command vu_noise_calibrate complete left=%u right=%u applied=%s\n",
        left_floor,
        right_floor,
        applied ? "yes" : "no_gyver_vu_active");
    if (length > 0 && static_cast<std::size_t>(length) < g_telemetry_line.size()) {
        (void)write_usb_line(g_telemetry_line.data(), static_cast<std::size_t>(length));
    }
}

void update_vu_calibration(const AudioLevelFrame& frame) {
    if (!g_vu_calibration_active) {
        return;
    }

    g_vu_calibration_left_peak = std::max(g_vu_calibration_left_peak,
                                          frame.left_peak);
    g_vu_calibration_right_peak = std::max(g_vu_calibration_right_peak,
                                           frame.right_peak);
    if (time_us_64() >= g_vu_calibration_deadline_us) {
        g_vu_calibration_active = false;
        apply_vu_calibration();
    }
}

void handle_command_line() {
    g_command_line[g_command_length] = '\0';
    if (std::strcmp(g_command_line.data(), "vu_noise_floors") == 0) {
        print_vu_floors();
        return;
    }

    if (std::strcmp(g_command_line.data(), "vu_noise_calibrate cancel") == 0) {
        if (g_vu_calibration_active) {
            g_vu_calibration_active = false;
            print_command_reply("vu_noise_calibrate cancelled");
        } else {
            print_command_reply("vu_noise_calibrate inactive");
        }
        return;
    }

    constexpr const char kCalibrationPrefix[] = "vu_noise_calibrate";
    constexpr std::size_t kCalibrationPrefixLength = sizeof(kCalibrationPrefix) - 1u;
    if (std::strncmp(g_command_line.data(),
                     kCalibrationPrefix,
                     kCalibrationPrefixLength) == 0 &&
        (g_command_line[kCalibrationPrefixLength] == '\0' ||
         g_command_line[kCalibrationPrefixLength] == ' ')) {
        uint32_t duration_ms = kDefaultVuCalibrationDurationMs;
        if (g_command_line[kCalibrationPrefixLength] == ' ') {
            char* end = nullptr;
            const unsigned long parsed = std::strtoul(
                g_command_line.data() + kCalibrationPrefixLength + 1u, &end, 10);
            if (end == g_command_line.data() + kCalibrationPrefixLength + 1u ||
                *end != '\0' || parsed < kMinimumVuCalibrationDurationMs ||
                parsed > kMaximumVuCalibrationDurationMs) {
                print_command_reply("vu_noise_calibrate invalid_duration");
                return;
            }
            duration_ms = static_cast<uint32_t>(parsed);
        }
        begin_vu_calibration(duration_ms);
        return;
    }

    print_command_reply("rejected unknown_command");
}

void service_usb_commands() {
    constexpr std::size_t kMaximumCharactersPerLoop = 16u;
    for (std::size_t count = 0u;
         count < kMaximumCharactersPerLoop && tud_cdc_available() > 0u;
         ++count) {
        const int character = tud_cdc_read_char();
        if (character < 0) {
            return;
        }

        if (character == '\r' || character == '\n') {
            if (g_command_discard_until_newline) {
                g_command_discard_until_newline = false;
            } else if (g_command_length != 0u) {
                handle_command_line();
            }
            g_command_length = 0u;
            continue;
        }

        if (g_command_discard_until_newline) {
            continue;
        }

        if (g_command_length + 1u >= g_command_line.size()) {
            g_command_discard_until_newline = true;
            g_command_length = 0u;
            print_command_reply("rejected line_too_long");
            continue;
        }

        g_command_line[g_command_length++] = static_cast<char>(character);
    }
}

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

void print_vu_telemetry() {
    DiagnosticEffectRuntimeSnapshot runtime{};
    if (!g_renderer_available ||
        !diagnostic_renderer_read_effect_runtime(0u, runtime)) {
        return;
    }

    const uint16_t left_effective = g_audio_frame.left_peak >
                                             runtime.config.gyver_left_noise_floor
                                         ? static_cast<uint16_t>(
                                               g_audio_frame.left_peak -
                                               runtime.config.gyver_left_noise_floor)
                                         : 0u;
    const uint16_t right_effective = g_audio_frame.right_peak >
                                              runtime.config.gyver_right_noise_floor
                                          ? static_cast<uint16_t>(
                                                g_audio_frame.right_peak -
                                                runtime.config.gyver_right_noise_floor)
                                          : 0u;
    const int length = std::snprintf(
        g_telemetry_line.data(),
        g_telemetry_line.size(),
        "DBG vu raw_l=%u raw_r=%u effective_l=%u effective_r=%u gate_l=%u gate_r=%u "
        "ref_l=%u ref_r=%u floor_l=%u floor_r=%u hysteresis=%u calibration=%s\n",
        g_audio_frame.left_peak,
        g_audio_frame.right_peak,
        left_effective,
        right_effective,
        runtime.left_gate_open ? 1u : 0u,
        runtime.right_gate_open ? 1u : 0u,
        runtime.left_reference,
        runtime.right_reference,
        runtime.config.gyver_left_noise_floor,
        runtime.config.gyver_right_noise_floor,
        runtime.config.gyver_noise_gate_hysteresis,
        g_vu_calibration_active ? "running" : "idle");
    if (length > 0 && static_cast<std::size_t>(length) < g_telemetry_line.size()) {
        (void)write_usb_line(g_telemetry_line.data(), static_cast<std::size_t>(length));
    }
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
        service_usb_commands();

        const uint64_t audio_work_start_us = time_us_64();
        if (audio_capture_process(g_audio_frame, g_centered_mono)) {
            update_vu_calibration(g_audio_frame);
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
        // Startup reporting is best effort. A disconnected or full CDC link
        // must not suppress later bounded telemetry or command processing.
        (void)try_deliver_startup_report();

        if (last_telemetry_us == 0u) {
            last_telemetry_us = now_us;
        } else if (now_us - last_telemetry_us >= kDiagnosticsPeriodUs) {
            last_telemetry_us = now_us;
            print_telemetry();
            print_vu_telemetry();
        }

        tight_loop_contents();
    }
}
