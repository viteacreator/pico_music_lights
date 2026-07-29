#include "audio/audio_processing.hpp"

#include <algorithm>

namespace {

constexpr uint16_t kAdcMaximum = 4095;
constexpr uint16_t kAttackShift = 1;
constexpr uint16_t kReleaseShift = 4;
constexpr uint16_t kDcAdaptationShift = 6;

uint16_t integer_square_root(uint32_t value) {
    uint32_t result = 0;
    uint32_t bit = 1u << 30;

    while (bit > value) {
        bit >>= 2;
    }
    while (bit != 0) {
        if (value >= result + bit) {
            value -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }
    return static_cast<uint16_t>(result);
}

uint16_t update_envelope(uint16_t previous, uint16_t level) {
    if (level > previous) {
        return static_cast<uint16_t>(previous + ((level - previous) >> kAttackShift));
    }
    return static_cast<uint16_t>(previous - ((previous - level) >> kReleaseShift));
}

uint16_t process_channel(AudioProcessor& processor, const uint16_t* samples,
                         unsigned channel, ChannelMetrics& metrics) {
    metrics = {0, 0, 0, 0, kAdcMaximum, 0, 0, 0};
    uint32_t raw_sum = 0;

    for (std::size_t index = 0; index < kAudioSamplesPerChannel; ++index) {
        const uint16_t sample = samples[index * kAudioChannels + channel] & kAdcMaximum;
        raw_sum += sample;
        metrics.raw_min = std::min(metrics.raw_min, sample);
        metrics.raw_max = std::max(metrics.raw_max, sample);
        metrics.clipping_low += sample == 0;
        metrics.clipping_high += sample == kAdcMaximum;
    }

    const int32_t mean = static_cast<int32_t>(raw_sum / kAudioSamplesPerChannel);
    if (!processor.initialized) {
        processor.dc_q8[channel] = mean << 8;
    } else {
        processor.dc_q8[channel] += ((mean << 8) - processor.dc_q8[channel]) >> kDcAdaptationShift;
    }
    metrics.dc_offset = static_cast<uint16_t>(processor.dc_q8[channel] >> 8);

    uint32_t sum_squares = 0;
    for (std::size_t index = 0; index < kAudioSamplesPerChannel; ++index) {
        const int32_t centered = static_cast<int32_t>(samples[index * kAudioChannels + channel] & kAdcMaximum) -
                                 (processor.dc_q8[channel] >> 8);
        const uint16_t absolute = static_cast<uint16_t>(centered < 0 ? -centered : centered);
        metrics.peak = std::max(metrics.peak, absolute);
        sum_squares += static_cast<uint32_t>(centered * centered);
    }

    metrics.rms = integer_square_root(sum_squares / kAudioSamplesPerChannel);
    processor.envelope[channel] = update_envelope(processor.envelope[channel], metrics.rms);
    metrics.envelope = processor.envelope[channel];
    return metrics.envelope;
}

}  // namespace

void audio_processor_reset(AudioProcessor& processor) {
    processor = {};
}

AudioLevelFrame process_audio_block(AudioProcessor& processor, const uint16_t* samples,
                                    uint32_t sequence, uint32_t dropped_blocks,
                                    CenteredMonoBlock* centered_mono_output) {
    AudioLevelFrame frame{};
    frame.sequence = sequence;
    frame.dropped_blocks = dropped_blocks;
    frame.left = process_channel(processor, samples, 0, frame.channels[0]);
    frame.right = process_channel(processor, samples, 1, frame.channels[1]);
    frame.aux = process_channel(processor, samples, 2, frame.channels[2]);

    uint32_t mono_sum_squares = 0;
    for (std::size_t index = 0; index < kAudioSamplesPerChannel; ++index) {
        const int32_t left = static_cast<int32_t>(samples[index * kAudioChannels] & kAdcMaximum) -
                             (processor.dc_q8[0] >> 8);
        const int32_t right = static_cast<int32_t>(samples[index * kAudioChannels + 1] & kAdcMaximum) -
                              (processor.dc_q8[1] >> 8);
        const int32_t mono = (left + right) / 2;

        if (centered_mono_output != nullptr) {
            centered_mono_output->samples[index] = static_cast<int16_t>(mono);
        }

        const uint16_t absolute = static_cast<uint16_t>(mono < 0 ? -mono : mono);
        frame.mono_metrics.peak = std::max(frame.mono_metrics.peak, absolute);
        mono_sum_squares += static_cast<uint32_t>(mono * mono);
    }
    frame.mono_metrics.rms = integer_square_root(mono_sum_squares / kAudioSamplesPerChannel);
    processor.mono_envelope = update_envelope(processor.mono_envelope, frame.mono_metrics.rms);
    frame.mono_metrics.envelope = processor.mono_envelope;
    frame.mono = frame.mono_metrics.envelope;

    if (centered_mono_output != nullptr) {
        centered_mono_output->sequence = sequence;
    }

    processor.initialized = true;
    frame.left_peak = frame.channels[0].peak;
    frame.right_peak = frame.channels[1].peak;
    frame.aux_peak = frame.channels[2].peak;
    frame.left_clipping = frame.channels[0].clipping_low != 0 || frame.channels[0].clipping_high != 0;
    frame.right_clipping = frame.channels[1].clipping_low != 0 || frame.channels[1].clipping_high != 0;
    frame.aux_clipping = frame.channels[2].clipping_low != 0 || frame.channels[2].clipping_high != 0;
    return frame;
}
