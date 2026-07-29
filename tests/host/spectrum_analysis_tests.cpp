#include "audio/spectrum_analyzer.hpp"
#include "audio/spectrum_resampler.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kSampleRateHz = 32000.0f;

CenteredMonoBlock make_tone_block(uint32_t fft_bin,
                                  uint32_t block_number,
                                  int16_t amplitude = 64) {
    CenteredMonoBlock block{};
    block.sequence = block_number;

    for (std::size_t sample = 0; sample < kAudioSamplesPerChannel; ++sample) {
        const std::size_t absolute_sample = block_number * kAudioSamplesPerChannel + sample;
        const float angle = 2.0f * kPi * static_cast<float>(fft_bin * absolute_sample) /
                            static_cast<float>(kSpectrumWindowSamples);
        block.samples[sample] = static_cast<int16_t>(
            static_cast<float>(amplitude) * std::sin(angle));
    }

    return block;
}

CenteredMonoBlock make_constant_block(uint32_t sequence, int16_t value) {
    CenteredMonoBlock block{};
    block.sequence = sequence;
    block.samples.fill(value);
    return block;
}

CenteredMonoBlock make_frequency_block(float frequency_hz,
                                      uint32_t block_number,
                                      int16_t amplitude = 64) {
    CenteredMonoBlock block{};
    block.sequence = block_number;

    for (std::size_t sample = 0; sample < kAudioSamplesPerChannel; ++sample) {
        const std::size_t absolute_sample = block_number * kAudioSamplesPerChannel + sample;
        const float angle = 2.0f * kPi * frequency_hz *
                            static_cast<float>(absolute_sample) / kSampleRateHz;
        block.samples[sample] = static_cast<int16_t>(amplitude * std::sin(angle));
    }

    return block;
}

bool push_window(SpectrumAnalyzer& analyzer,
                 uint32_t fft_bin,
                 uint32_t first_sequence,
                 SpectrumFrame& frame,
                 int16_t amplitude = 64) {
    bool ready = false;

    for (uint32_t block = 0; block < 4; ++block) {
        ready = analyzer.push(
            make_tone_block(fft_bin, first_sequence + block, amplitude),
            frame);
    }

    return ready;
}

bool push_frequency_window(SpectrumAnalyzer& analyzer,
                           float frequency_hz,
                           SpectrumFrame& frame,
                           int16_t amplitude = 64) {
    bool ready = false;
    for (uint32_t block = 0; block < 4; ++block) {
        ready = analyzer.push(make_frequency_block(frequency_hz, block, amplitude), frame);
    }
    return ready;
}

uint16_t maximum_band(const SpectrumFrame& frame) {
    uint16_t maximum = 0;

    for (uint16_t level : frame.bands) {
        maximum = std::max(maximum, level);
    }

    return maximum;
}

bool test_mapping_is_complete() {
    uint16_t expected_first = 1;

    for (const SpectrumBandRange& range : kSpectrumBandRanges) {
        if (range.first != expected_first || range.last < range.first) {
            return false;
        }

        expected_first = static_cast<uint16_t>(range.last + 1u);
    }

    return expected_first == kSpectrumPositiveBinLimit + 1u;
}

bool test_window_timing_and_overlap() {
    SpectrumAnalyzer analyzer{};
    SpectrumFrame frame{};

    for (uint32_t block = 0; block < 3; ++block) {
        if (analyzer.push(make_tone_block(16, block), frame)) {
            return false;
        }
    }

    if (!analyzer.push(make_tone_block(16, 3), frame)) {
        return false;
    }

    if (analyzer.push(make_tone_block(16, 4), frame)) {
        return false;
    }

    return analyzer.push(make_tone_block(16, 5), frame) && frame.sequence == 5;
}

bool test_hann_endpoints_and_dc_rejection() {
    SpectrumAnalyzer endpoint_analyzer{};
    SpectrumFrame endpoint_frame{};
    CenteredMonoBlock endpoint{};
    endpoint.sequence = 0;
    endpoint.samples[0] = 1000;

    for (uint32_t block = 0; block < 4; ++block) {
        endpoint.sequence = block;
        if (endpoint_analyzer.push(endpoint, endpoint_frame) && maximum_band(endpoint_frame) != 0) {
            return false;
        }
        endpoint.samples.fill(0);
    }

    SpectrumAnalyzer dc_analyzer{};
    SpectrumFrame dc_frame{};
    for (uint32_t block = 0; block < 4; ++block) {
        (void)dc_analyzer.push(make_constant_block(block, 500), dc_frame);
    }

    SpectrumAnalyzer centre_analyzer{};
    SpectrumFrame centre_frame{};
    CenteredMonoBlock centre{};
    for (uint32_t block = 0; block < 4; ++block) {
        centre.sequence = block;
        if (block == 2) {
            centre.samples[0] = 1000;
        }
        (void)centre_analyzer.push(centre, centre_frame);
        centre.samples.fill(0);
    }

    return maximum_band(dc_frame) == 0 && dc_frame.bass == 0 && dc_frame.low == 0 &&
           maximum_band(centre_frame) > 0;
}

bool test_noise_floor_and_reference_level() {
    SpectrumFrame below{};
    SpectrumFrame above{};
    SpectrumFrame reference{};
    SpectrumAnalyzer below_analyzer{};
    SpectrumAnalyzer above_analyzer{};
    SpectrumAnalyzer reference_analyzer{};

    if (!push_window(below_analyzer, 32, 0, below, 4) ||
        !push_window(above_analyzer, 32, 0, above, 8) ||
        !push_window(reference_analyzer, 32, 0, reference, 313)) {
        return false;
    }

    const uint16_t reference_first_frame = maximum_band(reference);
    return maximum_band(below) == 0 && maximum_band(above) > 0 &&
           reference_first_frame >= 30000 && reference_first_frame <= 34000;
}

bool test_requested_frequency_hz_classification() {
    SpectrumAnalyzer bass_analyzer{};
    SpectrumAnalyzer low_analyzer{};
    SpectrumAnalyzer mid_analyzer{};
    SpectrumAnalyzer high_analyzer{};
    SpectrumFrame bass{};
    SpectrumFrame low{};
    SpectrumFrame mid{};
    SpectrumFrame high{};

    return push_frequency_window(bass_analyzer, 80.0f, bass) && bass.bass > bass.low &&
           push_frequency_window(low_analyzer, 300.0f, low) && low.low > low.bass && low.low > low.mid &&
           push_frequency_window(mid_analyzer, 1000.0f, mid) && mid.mid > mid.low && mid.mid > mid.high &&
           push_frequency_window(high_analyzer, 6000.0f, high) && high.high > high.mid;
}

bool test_frequency_classification() {
    SpectrumFrame bass{};
    SpectrumFrame low{};
    SpectrumFrame mid{};
    SpectrumFrame high{};
    SpectrumAnalyzer bass_analyzer{};
    SpectrumAnalyzer low_analyzer{};
    SpectrumAnalyzer mid_analyzer{};
    SpectrumAnalyzer high_analyzer{};

    return push_window(bass_analyzer, 3, 0, bass) && bass.bass > bass.low &&
           push_window(low_analyzer, 10, 0, low) && low.low > low.bass && low.low > low.mid &&
           push_window(mid_analyzer, 32, 0, mid) && mid.mid > mid.low && mid.mid > mid.high &&
           push_window(high_analyzer, 192, 0, high) && high.high > high.mid;
}

bool test_mixed_tones_and_monotonic_amplitude() {
    SpectrumAnalyzer low_amplitude_analyzer{};
    SpectrumAnalyzer high_amplitude_analyzer{};
    SpectrumFrame low_amplitude{};
    SpectrumFrame high_amplitude{};

    if (!push_window(low_amplitude_analyzer, 32, 0, low_amplitude, 32) ||
        !push_window(high_amplitude_analyzer, 32, 0, high_amplitude, 64)) {
        return false;
    }

    SpectrumAnalyzer mixed_analyzer{};
    SpectrumFrame mixed{};
    for (uint32_t block = 0; block < 4; ++block) {
        CenteredMonoBlock input{};
        input.sequence = block;
        for (std::size_t sample = 0; sample < input.samples.size(); ++sample) {
            const std::size_t absolute = block * input.samples.size() + sample;
            input.samples[sample] = static_cast<int16_t>(
                32.0f * std::sin(2.0f * kPi * 3.0f * static_cast<float>(absolute) / 1024.0f) +
                32.0f * std::sin(2.0f * kPi * 32.0f * static_cast<float>(absolute) / 1024.0f));
        }
        (void)mixed_analyzer.push(input, mixed);
    }

    return maximum_band(high_amplitude) > maximum_band(low_amplitude) &&
           mixed.bass > 0 && mixed.mid > 0;
}

bool test_equal_amplitude_low_middle_high_response() {
    SpectrumFrame low{};
    SpectrumFrame middle{};
    SpectrumFrame high{};
    SpectrumAnalyzer low_analyzer{};
    SpectrumAnalyzer middle_analyzer{};
    SpectrumAnalyzer high_analyzer{};

    if (!push_window(low_analyzer, 4, 0, low) ||
        !push_window(middle_analyzer, 64, 0, middle) ||
        !push_window(high_analyzer, 300, 0, high)) {
        return false;
    }

    const uint16_t minimum = std::min({maximum_band(low), maximum_band(middle), maximum_band(high)});
    const uint16_t maximum = std::max({maximum_band(low), maximum_band(middle), maximum_band(high)});
    return minimum > 0 && maximum <= static_cast<uint16_t>(minimum * 2u);
}

bool test_smoothing_noise_floor_and_macro_ranges() {
    SpectrumAnalyzer analyzer{};
    SpectrumFrame first{};
    SpectrumFrame second{};
    SpectrumFrame silence{};

    if (!push_window(analyzer, 3, 0, first) || !push_window(analyzer, 3, 4, second)) {
        return false;
    }

    for (uint32_t block = 8; block < 10; ++block) {
        (void)analyzer.push(make_constant_block(block, 0), silence);
    }

    return second.bass > first.bass && silence.bass < second.bass &&
           first.bass > first.low && first.bass > first.mid && first.bass > first.high;
}

bool test_sequence_discontinuity() {
    SpectrumAnalyzer analyzer{};
    SpectrumFrame frame{};

    if (analyzer.push(make_tone_block(16, 1), frame)) {
        return false;
    }

    for (uint32_t sequence = 3; sequence <= 6; ++sequence) {
        (void)analyzer.push(make_tone_block(16, sequence), frame);
    }

    return frame.dropped_windows == 1 && frame.missing_audio_blocks == 1 && frame.sequence == 6;
}

bool test_resampling() {
    SpectrumFrame source{};
    for (std::size_t index = 0; index < kSpectrumBandCount; ++index) {
        source.bands[index] = static_cast<uint16_t>(index * 100u);
    }

    std::array<uint16_t, 5> five{};
    std::array<uint16_t, 8> eight{};
    std::array<uint16_t, 16> sixteen{};
    std::array<uint16_t, 32> thirty_two{};
    resample_spectrum(source, five.data(), five.size());
    resample_spectrum(source, eight.data(), eight.size());
    resample_spectrum(source, sixteen.data(), sixteen.size());
    resample_spectrum(source, thirty_two.data(), thirty_two.size());

    return five[0] < five[4] && eight[0] < eight[7] && sixteen[0] < sixteen[15] &&
           thirty_two[0] == 0 && thirty_two[17] == 1700 && thirty_two[31] == 3100;
}

}  // namespace

int main() {
    struct NamedTest { const char* name; bool (*run)(); };
    const std::array<NamedTest, 11> tests{{
        {"mapping", test_mapping_is_complete},
        {"window_overlap", test_window_timing_and_overlap},
        {"hann_dc", test_hann_endpoints_and_dc_rejection},
        {"noise_floor_reference", test_noise_floor_and_reference_level},
        {"bin_frequency_classification", test_frequency_classification},
        {"hz_frequency_classification", test_requested_frequency_hz_classification},
        {"mixed_monotonic", test_mixed_tones_and_monotonic_amplitude},
        {"equal_amplitude", test_equal_amplitude_low_middle_high_response},
        {"smoothing_macro", test_smoothing_noise_floor_and_macro_ranges},
        {"sequence_gap", test_sequence_discontinuity},
        {"resampling", test_resampling},
    }};

    for (const NamedTest& test : tests) {
        if (!test.run()) {
            std::fprintf(stderr, "Failed spectrum subtest: %s\n", test.name);
            return 1;
        }
    }
    return 0;
}
