#include "audio/spectrum_analyzer.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace {

// The Q15 backend applies a Hann window and scales each of ten radix-2 stages
// by one half. A returned positive-bin energy is consequently
// 2 * |DFT(windowed input) / 1024|^2 in squared ADC-count units. Dividing by
// Hann mean-square H produces the same normalized-energy units used by the
// original float formula: 2 * |DFT|^2 / (1024^2 * H).
constexpr float kHannPowerNormalization = 0.3746337890625f;

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

uint16_t convert_q15_energy_to_level(uint64_t q15_energy,
                                     uint16_t bin_count,
                                     uint16_t& previous) {
    // This is intentionally one of only 36 final float conversions. FFT,
    // power calculation, and bin aggregation are integer-only.
    const float normalized_energy =
        static_cast<float>(q15_energy) / kHannPowerNormalization;
    const float noise_energy =
        kSpectrumNoiseFloorPerBin * static_cast<float>(bin_count);
    const float cleaned_energy = std::max(0.0f, normalized_energy - noise_energy);
    const float normalized_level = 65535.0f * std::sqrt(
        cleaned_energy * kSpectrumGain / kSpectrumReferenceEnergy);
    const float clamped_level = std::clamp(normalized_level, 0.0f, 65535.0f);

    return smooth_level(static_cast<uint16_t>(clamped_level), previous);
}

uint64_t range_q15_energy(const spectrum_q15::Backend& backend,
                          SpectrumBandRange range) {
    uint64_t energy = 0;

    for (uint16_t bin = range.first; bin <= range.last; ++bin) {
        energy += backend.positive_bin_energy(bin);
    }

    return energy;
}

uint16_t range_bin_count(SpectrumBandRange range) {
    return static_cast<uint16_t>(range.last - range.first + 1u);
}

uint16_t select_dominant_bin(const spectrum_q15::Backend& backend,
                             uint64_t maximum_energy) {
    if (maximum_energy == 0u) {
        return 0u;
    }

    for (uint16_t first = 1u; first <= kSpectrumPositiveBinLimit; ++first) {
        if (backend.positive_bin_energy(first) != maximum_energy) {
            continue;
        }

        uint16_t last = first;
        while (last < kSpectrumPositiveBinLimit &&
               backend.positive_bin_energy(static_cast<uint16_t>(last + 1u)) ==
                   maximum_energy) {
            ++last;
        }

        // The lower centre gives a deterministic result for an even-width
        // plateau. If separated plateaus share the same maximum, the first
        // (lowest-frequency) plateau wins, retaining the prior tie policy.
        return static_cast<uint16_t>(first + (last - first) / 2u);
    }

    return 0u;
}

}  // namespace

const SpectrumDiagnostics& SpectrumAnalyzer::diagnostics() const {
    return diagnostics_;
}

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

    q15_backend_.transform(samples_);

    uint64_t total_q15_energy = 0;
    uint64_t dominant_q15_energy = 0;

    for (uint16_t bin = 1; bin <= kSpectrumPositiveBinLimit; ++bin) {
        const uint64_t energy = q15_backend_.positive_bin_energy(bin);
        total_q15_energy += energy;

        if (energy > dominant_q15_energy) {
            dominant_q15_energy = energy;
        }
    }

    diagnostics_.positive_bin_energy =
        static_cast<float>(total_q15_energy) / kHannPowerNormalization;
    diagnostics_.dominant_bin_power =
        static_cast<float>(dominant_q15_energy) / kHannPowerNormalization;
    diagnostics_.dominant_bin =
        select_dominant_bin(q15_backend_, dominant_q15_energy);

    for (std::size_t band = 0; band < kSpectrumBandRanges.size(); ++band) {
        const SpectrumBandRange range = kSpectrumBandRanges[band];
        output.bands[band] = convert_q15_energy_to_level(
            range_q15_energy(q15_backend_, range),
            range_bin_count(range),
            smoothed_levels_[band]);
    }

    constexpr SpectrumBandRange kBassRange{1, 5};
    constexpr SpectrumBandRange kLowRange{6, 16};
    constexpr SpectrumBandRange kMidRange{17, 80};
    constexpr SpectrumBandRange kHighRange{81, 384};

    output.bass = convert_q15_energy_to_level(
        range_q15_energy(q15_backend_, kBassRange),
        range_bin_count(kBassRange),
        smoothed_levels_[32]);
    output.low = convert_q15_energy_to_level(
        range_q15_energy(q15_backend_, kLowRange),
        range_bin_count(kLowRange),
        smoothed_levels_[33]);
    output.mid = convert_q15_energy_to_level(
        range_q15_energy(q15_backend_, kMidRange),
        range_bin_count(kMidRange),
        smoothed_levels_[34]);
    output.high = convert_q15_energy_to_level(
        range_q15_energy(q15_backend_, kHighRange),
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
