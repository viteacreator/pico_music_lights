#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>

#include "audio/audio_capture.hpp"
#include "audio/spectrum_analyzer.hpp"
#include "led/diagnostic_renderer.hpp"
#include "pico/stdlib.h"

namespace {

constexpr uint64_t kDiagnosticIntervalUs = 125000u;
constexpr uint32_t kNoiseMeasurementWindowCount = 192u;
constexpr std::size_t kCommandLength = 48u;

AudioLevelFrame g_audio_frame{};
CenteredMonoBlock g_centered_mono{};
SpectrumAnalyzer g_spectrum_analyzer;
SpectrumFrame g_spectrum_frame{};
uint32_t g_maximum_analysis_us = 0;
bool g_diagnostics_enabled = true;
char g_command[kCommandLength]{};
std::size_t g_command_length = 0;

struct StatisticsBaseline {
    uint32_t audio_dropped = 0;
    uint32_t fifo_overflow = 0;
    uint32_t fifo_underflow = 0;
    uint32_t spectrum_dropped = 0;
    uint32_t spectrum_missing = 0;
    uint32_t renderer_frames = 0;
    uint32_t renderer_skipped = 0;
};

struct NoiseMeasurement {
    bool active = false;
    uint32_t valid_windows = 0;
    uint32_t skipped_windows = 0;
    float sum_mean_power = 0.0f;
    float sum_maximum_power = 0.0f;
    float worst_maximum_power = 0.0f;
    std::array<uint32_t, 4> dominant_regions{};
    uint32_t previous_dropped_windows = 0;
    uint32_t previous_missing_blocks = 0;
};

StatisticsBaseline g_baseline{};
NoiseMeasurement g_noise_measurement{};

uint32_t delta(uint32_t current, uint32_t baseline) {
    return current - baseline;
}

uint32_t dominant_frequency_hz(uint16_t bin) {
    return static_cast<uint32_t>(
        (static_cast<uint32_t>(bin) * 32000u + 512u) / 1024u);
}

std::size_t dominant_region_index(uint16_t bin) {
    if (bin <= 5u) {
        return 0;
    }
    if (bin <= 16u) {
        return 1;
    }
    if (bin <= 80u) {
        return 2;
    }
    return 3;
}

void reset_statistics() {
    g_baseline.audio_dropped = g_audio_frame.dropped_blocks;
    g_baseline.fifo_overflow = audio_capture_fifo_errors();
    g_baseline.fifo_underflow = audio_capture_fifo_underflows();
    g_baseline.spectrum_dropped = g_spectrum_frame.dropped_windows;
    g_baseline.spectrum_missing = g_spectrum_frame.missing_audio_blocks;
    g_baseline.renderer_frames = diagnostic_renderer_stats().rendered_frames;
    g_baseline.renderer_skipped = diagnostic_renderer_stats().skipped_busy_frames;
    g_maximum_analysis_us = 0;
    diagnostic_renderer_reset_stats();
    g_baseline.renderer_frames = 0;
    g_baseline.renderer_skipped = 0;
    std::printf("Statistics reset\n");
}

void print_status() {
    const DiagnosticRendererStats& renderer = diagnostic_renderer_stats();
    std::printf("status: diagnostics %s, renderer %s, fft current %luus max %luus, "
                "spectrum windows %lu missing %lu, adc drop %lu over %lu under %lu, "
                "renderer frames %lu skipped %lu%s\n",
                g_diagnostics_enabled ? "on" : "off",
                diagnostic_renderer_is_enabled() ? "on" : "off",
                static_cast<unsigned long>(g_spectrum_frame.analysis_time_us),
                static_cast<unsigned long>(g_maximum_analysis_us),
                static_cast<unsigned long>(
                    delta(g_spectrum_frame.dropped_windows, g_baseline.spectrum_dropped)),
                static_cast<unsigned long>(
                    delta(g_spectrum_frame.missing_audio_blocks, g_baseline.spectrum_missing)),
                static_cast<unsigned long>(
                    delta(g_audio_frame.dropped_blocks, g_baseline.audio_dropped)),
                static_cast<unsigned long>(
                    delta(audio_capture_fifo_errors(), g_baseline.fifo_overflow)),
                static_cast<unsigned long>(
                    delta(audio_capture_fifo_underflows(), g_baseline.fifo_underflow)),
                static_cast<unsigned long>(
                    delta(renderer.rendered_frames, g_baseline.renderer_frames)),
                static_cast<unsigned long>(
                    delta(renderer.skipped_busy_frames, g_baseline.renderer_skipped)),
                g_noise_measurement.active ? ", noise measurement active" : "");
}

void print_diagnostics(const AudioLevelFrame& frame,
                       const SpectrumFrame& spectrum,
                       const SpectrumDiagnostics& raw) {
    const float mean_power = raw.positive_bin_energy /
                             static_cast<float>(kSpectrumPositiveBinLimit);
    const uint32_t frequency_hz = dominant_frequency_hz(raw.dominant_bin);

    std::printf("audio #%lu L %u R %u A %u M %u adc_drop %lu over %lu under %lu "
                "bass %u low %u mid %u high %u fft %luus max %luus windows %lu missing %lu "
                "raw_mean %.3f raw_max %.3f bin %u (%luHz) floor %.3f ref %.1f\n",
                static_cast<unsigned long>(frame.sequence),
                frame.left,
                frame.right,
                frame.aux,
                frame.mono,
                static_cast<unsigned long>(
                    delta(frame.dropped_blocks, g_baseline.audio_dropped)),
                static_cast<unsigned long>(
                    delta(audio_capture_fifo_errors(), g_baseline.fifo_overflow)),
                static_cast<unsigned long>(
                    delta(audio_capture_fifo_underflows(), g_baseline.fifo_underflow)),
                spectrum.bass,
                spectrum.low,
                spectrum.mid,
                spectrum.high,
                static_cast<unsigned long>(spectrum.analysis_time_us),
                static_cast<unsigned long>(g_maximum_analysis_us),
                static_cast<unsigned long>(
                    delta(spectrum.dropped_windows, g_baseline.spectrum_dropped)),
                static_cast<unsigned long>(
                    delta(spectrum.missing_audio_blocks, g_baseline.spectrum_missing)),
                static_cast<double>(mean_power),
                static_cast<double>(raw.dominant_bin_power),
                static_cast<unsigned>(raw.dominant_bin),
                static_cast<unsigned long>(frequency_hz),
                static_cast<double>(kSpectrumNoiseFloorPerBin),
                static_cast<double>(kSpectrumReferenceEnergy));
}

void print_noise_measurement() {
    if (g_noise_measurement.valid_windows == 0u) {
        std::printf("Noise measurement cancelled: no valid spectrum windows\n");
        return;
    }

    std::size_t dominant_region = 0;
    for (std::size_t index = 1; index < g_noise_measurement.dominant_regions.size(); ++index) {
        if (g_noise_measurement.dominant_regions[index] >
            g_noise_measurement.dominant_regions[dominant_region]) {
            dominant_region = index;
        }
    }

    constexpr std::array<const char*, 4> kRegionNames{{"bass", "low", "mid", "high"}};
    const float count = static_cast<float>(g_noise_measurement.valid_windows);
    std::printf("Noise measurement: valid %lu skipped %lu mean_bin %.3f avg_max %.3f "
                "worst_max %.3f dominant_region %s. Diagnostic evidence only; constants unchanged.\n",
                static_cast<unsigned long>(g_noise_measurement.valid_windows),
                static_cast<unsigned long>(g_noise_measurement.skipped_windows),
                static_cast<double>(g_noise_measurement.sum_mean_power / count),
                static_cast<double>(g_noise_measurement.sum_maximum_power / count),
                static_cast<double>(g_noise_measurement.worst_maximum_power),
                kRegionNames[dominant_region]);
}

void start_noise_measurement() {
    g_noise_measurement = {};
    g_noise_measurement.active = true;
    g_noise_measurement.previous_dropped_windows = g_spectrum_frame.dropped_windows;
    g_noise_measurement.previous_missing_blocks = g_spectrum_frame.missing_audio_blocks;
    std::printf("Noise measurement started: collecting %lu valid windows (about 3 seconds)\n",
                static_cast<unsigned long>(kNoiseMeasurementWindowCount));
}

void update_noise_measurement(const SpectrumFrame& spectrum,
                              const SpectrumDiagnostics& raw) {
    if (!g_noise_measurement.active) {
        return;
    }

    const bool discontinuity = spectrum.dropped_windows !=
                                   g_noise_measurement.previous_dropped_windows ||
                               spectrum.missing_audio_blocks !=
                                   g_noise_measurement.previous_missing_blocks;
    g_noise_measurement.previous_dropped_windows = spectrum.dropped_windows;
    g_noise_measurement.previous_missing_blocks = spectrum.missing_audio_blocks;

    if (discontinuity) {
        ++g_noise_measurement.skipped_windows;
        g_noise_measurement.valid_windows = 0;
        g_noise_measurement.sum_mean_power = 0.0f;
        g_noise_measurement.sum_maximum_power = 0.0f;
        g_noise_measurement.worst_maximum_power = 0.0f;
        g_noise_measurement.dominant_regions = {};
        std::printf("Noise measurement restarted after spectrum discontinuity\n");
        return;
    }

    const float mean_power = raw.positive_bin_energy /
                             static_cast<float>(kSpectrumPositiveBinLimit);
    g_noise_measurement.sum_mean_power += mean_power;
    g_noise_measurement.sum_maximum_power += raw.dominant_bin_power;
    g_noise_measurement.worst_maximum_power = std::max(
        g_noise_measurement.worst_maximum_power, raw.dominant_bin_power);
    ++g_noise_measurement.dominant_regions[dominant_region_index(raw.dominant_bin)];
    ++g_noise_measurement.valid_windows;

    if (g_noise_measurement.valid_windows >= kNoiseMeasurementWindowCount) {
        g_noise_measurement.active = false;
        print_noise_measurement();
    }
}

void print_help() {
    std::printf("Commands: status | stats reset | diagnostics on|off | renderer on|off | "
                "noise measure | noise cancel | help\n");
}

void process_command(const char* command) {
    if (std::strcmp(command, "status") == 0) {
        print_status();
    } else if (std::strcmp(command, "stats reset") == 0) {
        reset_statistics();
    } else if (std::strcmp(command, "diagnostics on") == 0) {
        g_diagnostics_enabled = true;
        std::printf("Periodic diagnostics enabled\n");
    } else if (std::strcmp(command, "diagnostics off") == 0) {
        g_diagnostics_enabled = false;
        std::printf("Periodic diagnostics disabled\n");
    } else if (std::strcmp(command, "renderer on") == 0) {
        diagnostic_renderer_set_enabled(true);
    } else if (std::strcmp(command, "renderer off") == 0) {
        diagnostic_renderer_set_enabled(false);
    } else if (std::strcmp(command, "noise measure") == 0) {
        if (g_noise_measurement.active) {
            std::printf("Noise measurement is already active\n");
        } else {
            start_noise_measurement();
        }
    } else if (std::strcmp(command, "noise cancel") == 0) {
        if (g_noise_measurement.active) {
            g_noise_measurement.active = false;
            print_noise_measurement();
        } else {
            std::printf("No noise measurement is active\n");
        }
    } else if (std::strcmp(command, "help") == 0) {
        print_help();
    } else {
        std::printf("Unknown command: %s\n", command);
        print_help();
    }
}

void poll_commands() {
    for (;;) {
        const int input = getchar_timeout_us(0);

        if (input == PICO_ERROR_TIMEOUT) {
            return;
        }

        const char character = static_cast<char>(input);

        if (character == '\r' || character == '\n') {
            if (g_command_length != 0u) {
                g_command[g_command_length] = '\0';
                process_command(g_command);
                g_command_length = 0;
            }
            continue;
        }

        if (g_command_length + 1u >= sizeof(g_command)) {
            g_command_length = 0;
            std::printf("Command rejected: line too long\n");
            continue;
        }

        g_command[g_command_length] = character;
        ++g_command_length;
    }
}

}  // namespace

int main() {
    stdio_init_all();
    sleep_ms(1500);
    std::printf("Pico Music Lights: Feature 003 physical validation\n");

    if (!diagnostic_renderer_initialize() || !audio_capture_initialize()) {
        std::printf("Initialization failed\n");
        while (true) {
            tight_loop_contents();
        }
    }

    std::printf("Audio DMA channel %lu; ADC 96 ksample/s aggregate; renderer default off\n",
                static_cast<unsigned long>(audio_capture_dma_channel()));
    print_help();
    reset_statistics();

    uint64_t last_diagnostic_us = 0;
    while (true) {
        poll_commands();
        diagnostic_renderer_service();

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
                update_noise_measurement(g_spectrum_frame,
                                         g_spectrum_analyzer.diagnostics());
            }

            diagnostic_renderer_update(g_audio_frame, g_spectrum_frame);

            if (g_diagnostics_enabled &&
                time_us_64() - last_diagnostic_us > kDiagnosticIntervalUs) {
                last_diagnostic_us = time_us_64();
                print_diagnostics(g_audio_frame, g_spectrum_frame,
                                  g_spectrum_analyzer.diagnostics());
            }
        }

        tight_loop_contents();
    }
}
