#include <algorithm>
#include <cstdio>

#include "audio/audio_capture.hpp"
#include "audio/spectrum_analyzer.hpp"
#include "pico/stdlib.h"

extern "C" bool led_vu_initialize(void);
extern "C" void led_vu_update(const AudioLevelFrame* frame);

namespace {

constexpr uint64_t kDiagnosticIntervalUs = 125000;

AudioLevelFrame g_audio_frame{};
CenteredMonoBlock g_centered_mono{};
SpectrumAnalyzer g_spectrum_analyzer;
SpectrumFrame g_spectrum_frame{};
uint32_t g_maximum_analysis_us = 0;

void print_diagnostics(const AudioLevelFrame& frame,
                       const SpectrumFrame& spectrum) {
    std::printf("audio #%lu L %u R %u A %u M %u adc_drop %lu over %lu under %lu "
                "bass %u low %u mid %u high %u fft %luus max %luus windows %lu missing %lu\n",
                static_cast<unsigned long>(frame.sequence),
                frame.left,
                frame.right,
                frame.aux,
                frame.mono,
                static_cast<unsigned long>(frame.dropped_blocks),
                static_cast<unsigned long>(audio_capture_fifo_errors()),
                static_cast<unsigned long>(audio_capture_fifo_underflows()),
                spectrum.bass,
                spectrum.low,
                spectrum.mid,
                spectrum.high,
                static_cast<unsigned long>(spectrum.analysis_time_us),
                static_cast<unsigned long>(spectrum.maximum_analysis_time_us),
                static_cast<unsigned long>(spectrum.dropped_windows),
                static_cast<unsigned long>(spectrum.missing_audio_blocks));
}

}  // namespace

int main() {
    stdio_init_all();
    sleep_ms(1500);
    std::printf("Pico Music Lights: Audio Capture Bring-Up\n");

    if (!led_vu_initialize() || !audio_capture_initialize()) {
        std::printf("Initialization failed\n");
        while (true) {
            tight_loop_contents();
        }
    }

    std::printf("Audio DMA channel %lu; ADC 96 ksample/s aggregate\n",
                static_cast<unsigned long>(audio_capture_dma_channel()));

    uint64_t last_diagnostic_us = 0;
    while (true) {
        if (audio_capture_process(g_audio_frame, g_centered_mono)) {
            const uint64_t analysis_start_us = time_us_64();
            if (g_spectrum_analyzer.push(g_centered_mono, g_spectrum_frame)) {
                g_spectrum_frame.analysis_time_us =
                    static_cast<uint32_t>(time_us_64() - analysis_start_us);
                g_maximum_analysis_us = std::max(g_maximum_analysis_us,
                                                  g_spectrum_frame.analysis_time_us);
                g_spectrum_frame.maximum_analysis_time_us = g_maximum_analysis_us;
            }
            led_vu_update(&g_audio_frame);
            if (time_us_64() - last_diagnostic_us > kDiagnosticIntervalUs) {
                last_diagnostic_us = time_us_64();
                print_diagnostics(g_audio_frame, g_spectrum_frame);
            }
        } else {
            led_vu_update(nullptr);
        }
        tight_loop_contents();
    }
}
