#pragma once

#include "audio/audio_processing.hpp"
#include "audio/spectrum_q15_backend.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

constexpr std::size_t kSpectrumBandCount = 32;
constexpr std::size_t kSpectrumWindowSamples = 1024;
constexpr std::size_t kSpectrumPositiveBinLimit = 384;
constexpr float kSpectrumNoiseFloorPerBin = 1.0f;
constexpr float kSpectrumGain = 1.0f;
constexpr float kSpectrumReferenceEnergy = 32768.0f;

struct SpectrumBandRange {
    uint16_t first;
    uint16_t last;
};

constexpr std::array<SpectrumBandRange, kSpectrumBandCount> kSpectrumBandRanges{{
    {1, 1}, {2, 2}, {3, 3}, {4, 4}, {5, 5}, {6, 6}, {7, 7}, {8, 8},
    {9, 9}, {10, 11}, {12, 13}, {14, 16}, {17, 19}, {20, 22},
    {23, 26}, {27, 31}, {32, 36}, {37, 42}, {43, 49}, {50, 58},
    {59, 68}, {69, 80}, {81, 94}, {95, 110}, {111, 129}, {130, 151},
    {152, 177}, {178, 207}, {208, 242}, {243, 283}, {284, 331},
    {332, 384},
}};

struct SpectrumFrame {
    std::array<uint16_t, kSpectrumBandCount> bands{};
    uint16_t bass = 0;
    uint16_t low = 0;
    uint16_t mid = 0;
    uint16_t high = 0;
    uint32_t sequence = 0;
    uint32_t analysis_time_us = 0;
    uint32_t maximum_analysis_time_us = 0;
    uint32_t dropped_windows = 0;
    uint32_t missing_audio_blocks = 0;
};

// Raw normalized FFT information before noise-floor subtraction, gain,
// level conversion, or smoothing. This is intended for diagnostics only.
struct SpectrumDiagnostics {
    float positive_bin_energy = 0.0f;
    float dominant_bin_power = 0.0f;
    uint16_t dominant_bin = 0;
};

class SpectrumAnalyzer {
public:
    bool push(const CenteredMonoBlock& block, SpectrumFrame& output);
    const SpectrumDiagnostics& diagnostics() const;

private:
    std::array<int16_t, kSpectrumWindowSamples> samples_{};
    spectrum_q15::Backend q15_backend_{};
    std::array<uint16_t, kSpectrumBandCount + 4> smoothed_levels_{};
    SpectrumDiagnostics diagnostics_{};
    std::size_t sample_count_ = 0;
    uint32_t last_input_sequence_ = 0;
    uint32_t dropped_windows_ = 0;
    uint32_t missing_audio_blocks_ = 0;
    bool have_input_sequence_ = false;
};
