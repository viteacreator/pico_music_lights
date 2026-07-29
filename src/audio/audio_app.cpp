#include <algorithm>
#include <cstdio>

#include "audio/audio_capture.hpp"
#include "audio/spectrum_analyzer.hpp"
#include "pico/stdlib.h"

extern "C" bool led_vu_initialize(void);
extern "C" void led_vu_update(const AudioLevelFrame* frame);

namespace {

constexpr uint64_t kDiagnosticIntervalUs = 125000;

void print_diagnostics(const AudioLevelFrame& frame) {
    std::printf("audio #%lu L %u R %u A %u M %u drop %lu over %lu under %lu\n",
                static_cast<unsigned long>(frame.sequence),
                frame.left,
                frame.right,
                frame.aux,
                frame.mono,
                static_cast<unsigned long>(frame.dropped_blocks),
                static_cast<unsigned long>(audio_capture_fifo_errors()),
                static_cast<unsigned long>(audio_capture_fifo_underflows()));
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

    AudioLevelFrame frame{};
    CenteredMonoBlock centered_mono{};
    SpectrumAnalyzer spectrum_analyzer;
    SpectrumFrame spectrum_frame{};
    uint64_t last_diagnostic_us = 0;
    uint32_t maximum_analysis_us = 0;
    while (true) {
        if (audio_capture_process(frame, centered_mono)) {
            const uint64_t analysis_start_us = time_us_64();
            if (spectrum_analyzer.push(centered_mono, spectrum_frame)) {
                spectrum_frame.analysis_time_us =
                    static_cast<uint32_t>(time_us_64() - analysis_start_us);
                maximum_analysis_us = std::max(maximum_analysis_us,
                                                spectrum_frame.analysis_time_us);
                spectrum_frame.maximum_analysis_time_us = maximum_analysis_us;
            }
            led_vu_update(&frame);
            if (time_us_64() - last_diagnostic_us > kDiagnosticIntervalUs) {
                last_diagnostic_us = time_us_64();
                print_diagnostics(frame);
            }
        } else {
            led_vu_update(nullptr);
        }
        tight_loop_contents();
    }
}
