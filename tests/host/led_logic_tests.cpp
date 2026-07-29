#include "led/channel_order.hpp"
#include "led/led_status.hpp"
#include "led/led_strip.hpp"
#include "led/led_output_conversion.hpp"
#include "led/rgbw_color.hpp"
#include "led/rgbw_conversion.hpp"
#include "audio/audio_processing.hpp"
#include "audio/spectrum_analyzer.hpp"
#include "audio/spectrum_resampler.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace {

constexpr uint16_t kAdcMidpoint = 2048;
constexpr float kPi = 3.14159265358979323846f;

bool test_led_logic() {
    const RgbwColor color{1, 2, 3, 4};
    const bool valid_statuses = InitializationResult::ok != InitializationResult::failed &&
                                InitializationResult::partial_success != InitializationResult::failed &&
                                LedStatus::busy != LedStatus::ok;
    const RgbwColor scaled = scale_rgbw({255, 128, 1, 64}, 128);
    const bool conversion_ok =
        pack_rgbw({0x11, 0x22, 0x33, 0x44}, ChannelOrder::rgbw) == 0x11223344u &&
        pack_rgbw({0x11, 0x22, 0x33, 0x44}, ChannelOrder::grbw) == 0x22113344u &&
        scale_channel(0, 127) == 0 && scale_channel(200, 255) == 200 &&
        scaled.red == 128 && scaled.green == 64 && scaled.blue == 1 && scaled.white == 32;

    LedStrip strip;
    RgbwColor logical[2]{};
    uint32_t packed[2]{};
    const bool validation_ok =
        strip.configure({true, 0, 2, 32, ChannelOrder::rgbw, false}) == LedStatus::invalid_pixel_count &&
        strip.configure({true, 301, 2, 32, ChannelOrder::rgbw, false}) == LedStatus::invalid_pixel_count &&
        strip.configure({true, 2, 2, 32, ChannelOrder::rgbw, true}) == LedStatus::ok &&
        strip.bind_slices(logical, packed, 2) == LedStatus::ok &&
        strip.set_pixel(0, {1, 2, 3, 4}) == LedStatus::ok &&
        strip.set_pixel(1, {5, 6, 7, 8}) == LedStatus::ok;
    RgbwColor first{};
    const bool stable_indexing_ok = strip.get_pixel(0, first) == LedStatus::ok && first.red == 1 &&
                                    strip.get_pixel(2, first) == LedStatus::invalid_pixel_index &&
                                    strip.clear() == LedStatus::ok && logical[0].red == 0 &&
                                    logical[1].white == 0;
    logical[0] = {10, 0, 0, 0};
    logical[1] = {20, 0, 0, 0};
    const bool reversed_output_ok = pack_strip_for_output(strip) == LedStatus::ok &&
                                    packed[0] == 0x03000000u && packed[1] == 0x01000000u &&
                                    logical[0].red == 10 && logical[1].red == 20;

    return sizeof(color) == 4 && ChannelOrder::rgbw != ChannelOrder::grbw && valid_statuses &&
           conversion_ok && validation_ok && stable_indexing_ok && reversed_output_ok;
}

void fill_constant_block(std::array<uint16_t, kAudioInterleavedSamples>& block,
                         uint16_t left,
                         uint16_t right,
                         uint16_t aux) {
    for (std::size_t index = 0; index < kAudioSamplesPerChannel; ++index) {
        block[index * kAudioChannels] = left;
        block[index * kAudioChannels + 1] = right;
        block[index * kAudioChannels + 2] = aux;
    }
}

bool test_audio_deinterleaving_and_offsets() {
    std::array<uint16_t, kAudioInterleavedSamples> block{};
    fill_constant_block(block, 2000, 2100, 2200);
    AudioProcessor processor{};
    const AudioLevelFrame frame = process_audio_block(processor, block.data(), 7, 3);

    return frame.channels[0].dc_offset == 2000 && frame.channels[1].dc_offset == 2100 &&
           frame.channels[2].dc_offset == 2200 && frame.left == 0 && frame.right == 0 &&
           frame.aux == 0 && frame.mono == 0 && frame.sequence == 7 &&
           frame.dropped_blocks == 3;
}

bool test_audio_peak_rms_mono_and_cancellation() {
    std::array<uint16_t, kAudioInterleavedSamples> block{};
    for (std::size_t index = 0; index < kAudioSamplesPerChannel; ++index) {
        const int16_t sample = (index & 1u) == 0u ? 100 : -100;
        block[index * kAudioChannels] = static_cast<uint16_t>(kAdcMidpoint + sample);
        block[index * kAudioChannels + 1] = static_cast<uint16_t>(kAdcMidpoint + sample);
        block[index * kAudioChannels + 2] = static_cast<uint16_t>(kAdcMidpoint - sample / 2);
    }
    AudioProcessor processor{};
    CenteredMonoBlock centered_mono{};
    const AudioLevelFrame in_phase =
        process_audio_block(processor, block.data(), 1, 0, &centered_mono);

    for (std::size_t index = 0; index < kAudioSamplesPerChannel; ++index) {
        const int16_t sample = (index & 1u) == 0u ? 100 : -100;
        block[index * kAudioChannels] = static_cast<uint16_t>(kAdcMidpoint + sample);
        block[index * kAudioChannels + 1] = static_cast<uint16_t>(kAdcMidpoint - sample);
    }
    AudioProcessor cancelling_processor{};
    const AudioLevelFrame cancelling = process_audio_block(cancelling_processor, block.data(), 2, 0);

    return in_phase.channels[0].peak == 100 && in_phase.channels[0].rms == 100 &&
           in_phase.channels[1].peak == 100 && in_phase.channels[1].rms == 100 &&
           in_phase.mono_metrics.peak == 100 && in_phase.mono_metrics.rms == 100 &&
           centered_mono.sequence == 1 && centered_mono.samples[0] == 100 &&
           centered_mono.samples[1] == -100 &&
           cancelling.mono_metrics.peak == 0 && cancelling.mono_metrics.rms == 0;
}

bool test_audio_dc_convergence_envelope_and_clipping() {
    std::array<uint16_t, kAudioInterleavedSamples> block{};
    AudioProcessor processor{};
    fill_constant_block(block, 2000, 2000, 2000);
    (void)process_audio_block(processor, block.data(), 1, 0);
    fill_constant_block(block, 2064, 2000, 2000);
    const AudioLevelFrame changed_dc = process_audio_block(processor, block.data(), 2, 0);

    for (std::size_t index = 0; index < kAudioSamplesPerChannel; ++index) {
        block[index * kAudioChannels] = (index & 1u) == 0u ? 0 : 4095;
        block[index * kAudioChannels + 1] = kAdcMidpoint;
        block[index * kAudioChannels + 2] = kAdcMidpoint;
    }
    AudioProcessor clipping_processor{};
    const AudioLevelFrame clipping = process_audio_block(clipping_processor, block.data(), 3, 0);

    return changed_dc.channels[0].dc_offset == 2001 && changed_dc.channels[0].rms == 63 &&
           clipping.channels[0].clipping_low == 128 && clipping.channels[0].clipping_high == 128 &&
           clipping.left_clipping;
}

bool test_audio_attack_is_faster_than_release() {
    std::array<uint16_t, kAudioInterleavedSamples> block{};
    for (std::size_t index = 0; index < kAudioSamplesPerChannel; ++index) {
        const int16_t sample = (index & 1u) == 0u ? 100 : -100;
        block[index * kAudioChannels] = static_cast<uint16_t>(kAdcMidpoint + sample);
        block[index * kAudioChannels + 1] = kAdcMidpoint;
        block[index * kAudioChannels + 2] = kAdcMidpoint;
    }

    AudioProcessor processor{};
    const AudioLevelFrame rising = process_audio_block(processor, block.data(), 1, 0);
    fill_constant_block(block, kAdcMidpoint, kAdcMidpoint, kAdcMidpoint);
    const AudioLevelFrame falling = process_audio_block(processor, block.data(), 2, 0);

    const uint16_t attack_delta = rising.channels[0].envelope;
    const uint16_t release_delta = static_cast<uint16_t>(
        rising.channels[0].envelope - falling.channels[0].envelope);
    return rising.channels[0].rms == 100 && falling.channels[0].rms == 0 &&
           attack_delta > release_delta;
}

CenteredMonoBlock make_tone_block(uint32_t fft_bin, uint32_t block_number) {
    CenteredMonoBlock block{};
    block.sequence = block_number;
    for (std::size_t sample = 0; sample < kAudioSamplesPerChannel; ++sample) {
        const std::size_t absolute_sample =
            block_number * kAudioSamplesPerChannel + sample;
        const float angle = 2.0f * kPi * static_cast<float>(fft_bin * absolute_sample) /
                            static_cast<float>(kSpectrumWindowSamples);
        block.samples[sample] = static_cast<int16_t>(64.0f * std::sin(angle));
    }
    return block;
}

bool analyze_bin_centered_tone(uint32_t fft_bin, SpectrumFrame& frame) {
    SpectrumAnalyzer analyzer{};
    bool ready = false;
    for (uint32_t block = 0; block < 4; ++block) {
        const CenteredMonoBlock input = make_tone_block(fft_bin, block);
        ready = analyzer.push(input, frame);
    }
    return ready;
}

bool test_spectrum_bin_ranges_and_tone_energy() {
    // This direct table check requires the public frozen mapping contract.
    uint16_t expected_first = 1;
    for (const SpectrumBinRange& range : kSpectrumBandRanges) {
        if (range.first != expected_first || range.last < range.first) {
            return false;
        }
        expected_first = static_cast<uint16_t>(range.last + 1);
    }
    if (expected_first != 385) {
        return false;
    }

    SpectrumFrame low{};
    SpectrumFrame middle{};
    SpectrumFrame high{};
    if (!analyze_bin_centered_tone(4, low) || !analyze_bin_centered_tone(64, middle) ||
        !analyze_bin_centered_tone(300, high)) {
        return false;
    }

    const auto maximum_band = [](const SpectrumFrame& frame) {
        uint16_t maximum = 0;
        for (const uint16_t level : frame.bands) {
            maximum = std::max(maximum, level);
        }
        return maximum;
    };
    const uint16_t low_level = maximum_band(low);
    const uint16_t middle_level = maximum_band(middle);
    const uint16_t high_level = maximum_band(high);
    const uint16_t minimum = std::min({low_level, middle_level, high_level});
    const uint16_t maximum = std::max({low_level, middle_level, high_level});

    // Equal-amplitude bin-centred tones must not lose level solely because a
    // high-frequency display band contains more FFT bins. A factor-of-two
    // tolerance permits integer rounding and fixed-point-compatible backends.
    return minimum > 0 && maximum <= static_cast<uint16_t>(minimum * 2u);
}

bool test_spectrum_sequence_gap_accounting() {
    SpectrumAnalyzer analyzer{};
    SpectrumFrame frame{};

    if (analyzer.push(make_tone_block(16, 1), frame)) {
        return false;
    }

    bool ready = false;
    for (uint32_t sequence = 3; sequence <= 7; ++sequence) {
        ready = analyzer.push(make_tone_block(16, sequence), frame) || ready;
    }

    return ready && frame.dropped_windows >= 1 && frame.sequence >= 6;
}

bool test_spectrum_resampling() {
    SpectrumFrame source{};
    for (std::size_t index = 0; index < kSpectrumBandCount; ++index) {
        source.bands[index] = static_cast<uint16_t>(index * 100u);
    }

    std::array<uint16_t, 32> direct{};
    std::array<uint16_t, 16> paired{};
    resample_spectrum(source, direct.data(), direct.size());
    resample_spectrum(source, paired.data(), paired.size());

    return direct[0] == 0 && direct[17] == 1700 && direct[31] == 3100 &&
           paired[0] == 50 && paired[7] == 1450 && paired[15] == 3050;
}

}  // namespace

int main() {
    return test_led_logic() && test_audio_deinterleaving_and_offsets() &&
                   test_audio_peak_rms_mono_and_cancellation() &&
                   test_audio_dc_convergence_envelope_and_clipping() &&
                   test_audio_attack_is_faster_than_release() &&
                   test_spectrum_bin_ranges_and_tone_energy() &&
                   test_spectrum_sequence_gap_accounting() &&
                   test_spectrum_resampling()
               ? 0
               : 1;
}
