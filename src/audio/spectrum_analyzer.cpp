#include "audio/spectrum_analyzer.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace {

constexpr float kHannPowerNormalization = 0.3746337890625f;
constexpr float kFftLengthSquared =
    static_cast<float>(kSpectrumWindowSamples * kSpectrumWindowSamples);
constexpr float kPositiveBinPowerScale =
    2.0f / (kFftLengthSquared * kHannPowerNormalization);

// This provisional floor suppresses residual ADC and floating-point noise.
// It is measured in normalized single-bin FFT-power units and requires
// physical noise measurements before final tuning.
constexpr float kNoiseFloorPerBin = 1.0f;
constexpr float kSpectrumGain = 1.0f;
// A bin-centred sine with approximately 313 centered ADC counts amplitude
// produces this normalized energy after the Hann/FFT normalization.
constexpr float kReferenceEnergy = 32768.0f;
constexpr float kHannStepCosine = 0.9999811384617630f;
constexpr float kHannStepSine = 0.0061418825059791f;

struct ComplexCoefficient {
    float real;
    float imaginary;
};

// One base coefficient per radix-2 stage. These flash-resident constants
// remove all sine/cosine calls from the FFT itself. Each stage generates its
// successive roots by complex multiplication.
constexpr std::array<ComplexCoefficient, 10> kStageTwiddles{{
    {-1.0f, 0.0f},
    {0.0f, -1.0f},
    {0.7071067811865475f, -0.7071067811865475f},
    {0.9238795325112867f, -0.3826834323650898f},
    {0.9807852804032304f, -0.19509032201612825f},
    {0.9951847266721969f, -0.0980171403295606f},
    {0.9987954562051724f, -0.0490676743274180f},
    {0.9996988186962042f, -0.0245412285229123f},
    {0.9999247018391445f, -0.0122715382857199f},
    {0.9999811752826011f, -0.0061358846491545f},
}};

uint16_t smooth_level(uint16_t target, uint16_t& previous) {
    if (target > previous) {
        previous = static_cast<uint16_t>(
            previous + ((static_cast<uint32_t>(target) - previous) >> 1));
    } else {
        previous = static_cast<uint16_t>(
            previous - ((static_cast<uint32_t>(previous) - target) >> 4));
    }

    return previous;
}

uint16_t convert_energy_to_level(float energy,
                                 uint16_t bin_count,
                                 uint16_t& previous) {
    const float noise_energy =
        kNoiseFloorPerBin * static_cast<float>(bin_count);
    const float cleaned_energy = std::max(0.0f, energy - noise_energy);
    const float normalized_energy =
        cleaned_energy * kSpectrumGain / kReferenceEnergy;
    const float normalized_level =
        65535.0f * std::sqrt(normalized_energy);
    const float clamped_level = std::clamp(normalized_level, 0.0f, 65535.0f);

    return smooth_level(static_cast<uint16_t>(clamped_level), previous);
}

void execute_fft(std::array<float, kSpectrumWindowSamples>& real,
                 std::array<float, kSpectrumWindowSamples>& imaginary) {
    for (unsigned index = 1, reversed = 0;
         index < kSpectrumWindowSamples;
         ++index) {
        unsigned bit = kSpectrumWindowSamples >> 1u;

        while ((reversed & bit) != 0u) {
            reversed ^= bit;
            bit >>= 1u;
        }

        reversed ^= bit;

        if (index < reversed) {
            std::swap(real[index], real[reversed]);
            std::swap(imaginary[index], imaginary[reversed]);
        }
    }

    for (unsigned length = 2, stage = 0;
         length <= kSpectrumWindowSamples;
         length <<= 1u, ++stage) {
        const ComplexCoefficient root = kStageTwiddles[stage];
        const unsigned half_length = length >> 1u;

        for (unsigned block = 0;
             block < kSpectrumWindowSamples;
             block += length) {
            float twiddle_real = 1.0f;
            float twiddle_imaginary = 0.0f;

            for (unsigned offset = 0; offset < half_length; ++offset) {
                const unsigned even_index = block + offset;
                const unsigned odd_index = even_index + half_length;
                const float transformed_real =
                    twiddle_real * real[odd_index] -
                    twiddle_imaginary * imaginary[odd_index];
                const float transformed_imaginary =
                    twiddle_real * imaginary[odd_index] +
                    twiddle_imaginary * real[odd_index];
                const float even_real = real[even_index];
                const float even_imaginary = imaginary[even_index];

                real[odd_index] = even_real - transformed_real;
                imaginary[odd_index] = even_imaginary - transformed_imaginary;
                real[even_index] = even_real + transformed_real;
                imaginary[even_index] = even_imaginary + transformed_imaginary;

                const float previous_twiddle_real = twiddle_real;
                twiddle_real = previous_twiddle_real * root.real -
                               twiddle_imaginary * root.imaginary;
                twiddle_imaginary = previous_twiddle_real * root.imaginary +
                                    twiddle_imaginary * root.real;
            }
        }
    }
}

float normalized_bin_power(const std::array<float, kSpectrumWindowSamples>& real,
                           const std::array<float, kSpectrumWindowSamples>& imaginary,
                           uint16_t bin) {
    return (real[bin] * real[bin] + imaginary[bin] * imaginary[bin]) *
           kPositiveBinPowerScale;
}

float range_energy(const std::array<float, kSpectrumWindowSamples>& real,
                   const std::array<float, kSpectrumWindowSamples>& imaginary,
                   SpectrumBandRange range) {
    float energy = 0.0f;

    for (uint16_t bin = range.first; bin <= range.last; ++bin) {
        energy += normalized_bin_power(real, imaginary, bin);
    }

    return energy;
}

uint16_t range_bin_count(SpectrumBandRange range) {
    return static_cast<uint16_t>(range.last - range.first + 1u);
}

}  // namespace

bool SpectrumAnalyzer::push(const CenteredMonoBlock& block, SpectrumFrame& output) {
    if (have_input_sequence_ && block.sequence != last_input_sequence_ + 1u) {
        if (block.sequence > last_input_sequence_ + 1u) {
            missing_audio_blocks_ += block.sequence - last_input_sequence_ - 1u;
        }
        if (sample_count_ != 0) {
            ++dropped_windows_;
        }
        sample_count_ = 0;
    }

    last_input_sequence_ = block.sequence;
    have_input_sequence_ = true;

    for (int16_t sample : block.samples) {
        if (sample_count_ < kSpectrumWindowSamples) {
            samples_[sample_count_] = sample;
            ++sample_count_;
        }
    }

    if (sample_count_ < kSpectrumWindowSamples) {
        return false;
    }

    int64_t sample_sum = 0;

    for (int16_t sample : samples_) {
        sample_sum += sample;
    }

    const int32_t residual_mean = static_cast<int32_t>(
        sample_sum / static_cast<int64_t>(kSpectrumWindowSamples));
    float hann_phase_cosine = 1.0f;
    float hann_phase_sine = 0.0f;

    for (std::size_t index = 0; index < kSpectrumWindowSamples; ++index) {
        const float hann = 0.5f - 0.5f * hann_phase_cosine;
        const int32_t residual_centered =
            static_cast<int32_t>(samples_[index]) - residual_mean;
        real_[index] = static_cast<float>(residual_centered) * hann;
        imaginary_[index] = 0.0f;

        const float previous_phase_cosine = hann_phase_cosine;
        hann_phase_cosine = previous_phase_cosine * kHannStepCosine -
                            hann_phase_sine * kHannStepSine;
        hann_phase_sine = previous_phase_cosine * kHannStepSine +
                           hann_phase_sine * kHannStepCosine;
    }

    execute_fft(real_, imaginary_);

    for (std::size_t band = 0; band < kSpectrumBandRanges.size(); ++band) {
        const float energy = range_energy(real_, imaginary_, kSpectrumBandRanges[band]);
        output.bands[band] = convert_energy_to_level(
            energy,
            range_bin_count(kSpectrumBandRanges[band]),
            smoothed_levels_[band]);
    }

    constexpr SpectrumBandRange kBassRange{1, 5};
    constexpr SpectrumBandRange kLowRange{6, 16};
    constexpr SpectrumBandRange kMidRange{17, 80};
    constexpr SpectrumBandRange kHighRange{81, 384};

    output.bass = convert_energy_to_level(
        range_energy(real_, imaginary_, kBassRange),
        range_bin_count(kBassRange),
        smoothed_levels_[32]);
    output.low = convert_energy_to_level(
        range_energy(real_, imaginary_, kLowRange),
        range_bin_count(kLowRange),
        smoothed_levels_[33]);
    output.mid = convert_energy_to_level(
        range_energy(real_, imaginary_, kMidRange),
        range_bin_count(kMidRange),
        smoothed_levels_[34]);
    output.high = convert_energy_to_level(
        range_energy(real_, imaginary_, kHighRange),
        range_bin_count(kHighRange),
        smoothed_levels_[35]);

    output.sequence = block.sequence;
    output.analysis_time_us = 0;
    output.dropped_windows = dropped_windows_;
    output.missing_audio_blocks = missing_audio_blocks_;

    std::copy(samples_.begin() + (kSpectrumWindowSamples / 2u),
              samples_.end(),
              samples_.begin());
    sample_count_ = kSpectrumWindowSamples / 2u;

    return true;
}
