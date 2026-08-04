#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "audio/audio_capture.hpp"
#include "audio/idle_commands.hpp"
#include "audio/spectrum_analyzer.hpp"
#include "audio/vu_calibration.hpp"
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
    return vu_calibrated_floor(peak);
}

void print_vu_floors() {
    uint16_t left_floor = 0u;
    uint16_t right_floor = 0u;
    if (!g_renderer_available ||
        !diagnostic_renderer_volatile_gyver_vu_noise_floors(left_floor,
                                                             right_floor)) {
        print_command_reply("vu_noise_floors renderer=unavailable");
        return;
    }

    const uint16_t hysteresis = diagnostic_renderer_volatile_gyver_vu_hysteresis();
    const int length = std::snprintf(
        g_telemetry_line.data(),
        g_telemetry_line.size(),
        "DBG command vu_noise_floors measured_left=%u measured_right=%u "
        "floor_left=%u floor_right=%u hysteresis=%u mode=%s\n",
        g_vu_calibration_left_peak,
        g_vu_calibration_right_peak,
        left_floor,
        right_floor,
        hysteresis,
        (g_vu_calibration_last_left_floor == 0u &&
         g_vu_calibration_last_right_floor == 0u) ? "defaults" : "calibrated");
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
    if (!diagnostic_renderer_set_volatile_gyver_vu_noise_floors(left_floor,
                                                                  right_floor)) {
        print_command_reply("vu_noise_calibrate apply=failed");
        return;
    }

    g_vu_calibration_last_left_floor = left_floor;
    g_vu_calibration_last_right_floor = right_floor;
    const int length = std::snprintf(
        g_telemetry_line.data(),
        g_telemetry_line.size(),
        "DBG command vu_noise_calibrate complete left=%u right=%u applied=%s\n",
        left_floor,
        right_floor,
        "yes");
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

void print_idle_status() {
    effects::IdleLightingConfig config{};
    (void)diagnostic_renderer_read_idle_config(config);
    const effects::IdleLightingRuntime& runtime = diagnostic_renderer_idle_runtime();
    const int length = std::snprintf(
        g_telemetry_line.data(),
        g_telemetry_line.size(),
        "DBG command idle enabled=%u startup=%u state=%u mix=%u active=%u,%u,%u "
        "inactive_ms=%lu timeout_ms=%u confirm_ms=%u color=%u,%u,%u,%u brightness_q8=%u "
        "input_mask=0x%02x strip_mask=0x%02x floors=%u,%u,%u hysteresis=%u fades=%u,%u\n",
        config.enabled ? 1u : 0u,
        config.startup_idle_enabled ? 1u : 0u,
        static_cast<unsigned>(runtime.state),
        runtime.effect_mix,
        runtime.left_active ? 1u : 0u,
        runtime.right_active ? 1u : 0u,
        runtime.aux_active ? 1u : 0u,
        static_cast<unsigned long>(runtime.inactive_ms),
        config.silence_timeout_ms,
        config.audio_confirmation_ms,
        config.idle_color_rgbw.red,
        config.idle_color_rgbw.green,
        config.idle_color_rgbw.blue,
        config.idle_color_rgbw.white,
        config.idle_brightness_q8,
        static_cast<unsigned>(config.activity_input_mask),
        static_cast<unsigned>(config.strip_enable_mask),
        config.left_activity_floor,
        config.right_activity_floor,
        config.aux_activity_floor,
        config.activity_hysteresis,
        config.fade_to_effect_ms,
        config.fade_to_idle_ms);
    if (length > 0 && static_cast<std::size_t>(length) < g_telemetry_line.size()) {
        (void)write_usb_line(g_telemetry_line.data(), static_cast<std::size_t>(length));
    }
}

void stage_idle_enabled(bool enabled) {
    effects::IdleLightingConfig config{};
    (void)diagnostic_renderer_read_idle_config(config);
    config.enabled = enabled;
    if (diagnostic_renderer_stage_idle_config(config) == effects::IdleLightingStatus::ok) {
        print_command_reply(enabled ? "idle_enable staged" : "idle_disable staged");
    } else {
        print_command_reply("idle configuration rejected");
    }
}

void stage_idle_test_config() {
    effects::IdleLightingConfig config{};
    config.enabled = true;
    config.startup_idle_enabled = true;
    config.silence_timeout_ms = 10000u;
    config.audio_confirmation_ms = 150u;
    config.idle_color_rgbw = {0u, 0u, 0u, 255u};
    config.idle_brightness_q8 = effects::kEffectUnityGain;
    config.fade_to_effect_ms = 750u;
    config.fade_to_idle_ms = 1500u;
    config.activity_input_mask = effects::kIdleAllInputs;
    config.strip_enable_mask = effects::kIdleAllStrips;
    if (diagnostic_renderer_stage_idle_config(config) == effects::IdleLightingStatus::ok) {
        print_command_reply("idle_test staged");
    } else {
        print_command_reply("idle_test rejected");
    }
}

void handle_command_line() {
    g_command_line[g_command_length] = '\0';
    const IdleCommandType idle_command = parse_idle_command(g_command_line.data());
    if (idle_command == IdleCommandType::enable) {
        stage_idle_enabled(true);
        return;
    }
    if (idle_command == IdleCommandType::disable) {
        stage_idle_enabled(false);
        return;
    }
    if (idle_command == IdleCommandType::status) {
        print_idle_status();
        return;
    }
    if (idle_command == IdleCommandType::test) {
        stage_idle_test_config();
        return;
    }
    const VuCalibrationCommand command =
        parse_vu_calibration_command(g_command_line.data());
    if (command.type == VuCalibrationCommandType::report_floors) {
        print_vu_floors();
        return;
    }

    if (command.type == VuCalibrationCommandType::cancel) {
        if (g_vu_calibration_active) {
            g_vu_calibration_active = false;
            print_command_reply("vu_noise_calibrate cancelled");
        } else {
            print_command_reply("vu_noise_calibrate inactive");
        }
        return;
    }

    if (command.type == VuCalibrationCommandType::start) {
        begin_vu_calibration(command.duration_ms);
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
    const effects::IdleLightingRuntime& idle = diagnostic_renderer_idle_runtime();
    effects::IdleLightingConfig idle_config{};
    (void)diagnostic_renderer_read_idle_config(idle_config);
    int length = std::snprintf(
        g_telemetry_line.data(),
        g_telemetry_line.size(),
        "DBG core audio_seq=%lu L=%u R=%u Aux=%u Mono=%u adc_drop=%lu adc_over=%lu "
        "adc_under=%lu fft_us=%lu fft_max_us=%lu audio_work_us=%lu audio_work_max_us=%lu "
        "dropped_windows=%lu missing_audio_blocks=%lu\n",
        static_cast<unsigned long>(g_audio_frame.sequence),
        g_audio_frame.left,
        g_audio_frame.right,
        g_audio_frame.aux,
        g_audio_frame.mono,
        static_cast<unsigned long>(g_audio_frame.dropped_blocks),
        static_cast<unsigned long>(audio_capture_fifo_errors()),
        static_cast<unsigned long>(audio_capture_fifo_underflows()),
        static_cast<unsigned long>(g_spectrum_frame.analysis_time_us),
        static_cast<unsigned long>(g_maximum_analysis_us),
        static_cast<unsigned long>(g_audio_work_us),
        static_cast<unsigned long>(g_maximum_audio_work_us),
        static_cast<unsigned long>(g_spectrum_frame.dropped_windows),
        static_cast<unsigned long>(g_spectrum_frame.missing_audio_blocks));

    if (length > 0 && static_cast<std::size_t>(length) < g_telemetry_line.size()) {
        (void)write_usb_line(g_telemetry_line.data(), static_cast<std::size_t>(length));
    }

    length = std::snprintf(
        g_telemetry_line.data(),
        g_telemetry_line.size(),
        "DBG effects spectrum_seq=%lu bass=%u low=%u mid=%u high=%u scene=%s cfg_gen=%lu "
        "effect_us=%lu effect_max_us=%lu effect_skip=%lu effect_fail=%lu led_timeouts=%lu\n",
        static_cast<unsigned long>(g_spectrum_frame.sequence),
        g_spectrum_frame.bass,
        g_spectrum_frame.low,
        g_spectrum_frame.mid,
        g_spectrum_frame.high,
        diagnostic_renderer_active_scene_name(),
        static_cast<unsigned long>(diagnostic_renderer_configuration_generation()),
        static_cast<unsigned long>(renderer.effect_render_us),
        static_cast<unsigned long>(renderer.effect_render_max_us),
        static_cast<unsigned long>(renderer.effect_frames_skipped_busy),
        static_cast<unsigned long>(renderer.effect_frames_failed),
        static_cast<unsigned long>(renderer.led_frame_timeouts));

    if (length > 0 && static_cast<std::size_t>(length) < g_telemetry_line.size()) {
        (void)write_usb_line(g_telemetry_line.data(), static_cast<std::size_t>(length));
    }

    length = std::snprintf(
        g_telemetry_line.data(),
        g_telemetry_line.size(),
        "DBG idle enabled=%u state=%u mix=%u active_l=%u active_r=%u active_a=%u "
        "inactive_ms=%lu input_mask=0x%02x strip_mask=0x%02x timeout_ms=%u confirm_ms=%u\n",
        idle_config.enabled ? 1u : 0u,
        static_cast<unsigned>(idle.state),
        idle.effect_mix,
        idle.left_active ? 1u : 0u,
        idle.right_active ? 1u : 0u,
        idle.aux_active ? 1u : 0u,
        static_cast<unsigned long>(idle.inactive_ms),
        static_cast<unsigned>(idle_config.activity_input_mask),
        static_cast<unsigned>(idle_config.strip_enable_mask),
        idle_config.silence_timeout_ms,
        idle_config.audio_confirmation_ms);
    if (length > 0 && static_cast<std::size_t>(length) < g_telemetry_line.size()) {
        (void)write_usb_line(g_telemetry_line.data(), static_cast<std::size_t>(length));
    }
}

void print_vu_telemetry() {
    if (!g_renderer_available) {
        return;
    }

    DiagnosticEffectRuntimeSnapshot runtime{};
    std::size_t strip_index = effects::kEffectStripCount;
    for (std::size_t index = 0u; index < effects::kEffectStripCount; ++index) {
        if (!diagnostic_renderer_read_effect_runtime(index, runtime)) {
            continue;
        }
        if (runtime.config.type == effects::EffectType::gyver_vu_gradient ||
            runtime.config.type == effects::EffectType::gyver_vu_rainbow) {
            strip_index = index;
            break;
        }
    }
    if (strip_index == effects::kEffectStripCount) {
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
        "DBG vu strip=%u raw_l=%u raw_r=%u effective_l=%u effective_r=%u gate_l=%u gate_r=%u "
        "smooth_l=%u smooth_r=%u ref_l=%u ref_r=%u floor_l=%u floor_r=%u hysteresis=%u calibration=%s\n",
        static_cast<unsigned>(strip_index),
        g_audio_frame.left_peak,
        g_audio_frame.right_peak,
        left_effective,
        right_effective,
        runtime.left_gate_open ? 1u : 0u,
        runtime.right_gate_open ? 1u : 0u,
        runtime.left_smoothed,
        runtime.right_smoothed,
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
