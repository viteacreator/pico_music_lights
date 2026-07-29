#pragma once

#include <cstddef>
#include <array>
#include <cstdint>

constexpr std::size_t kAudioChannels = 3;
constexpr std::size_t kAudioSamplesPerChannel = 256;
constexpr std::size_t kAudioInterleavedSamples = 768;

struct CenteredMonoBlock {
    std::array<int16_t, kAudioSamplesPerChannel> samples{};
    uint32_t sequence = 0;
};

struct ChannelMetrics {
    uint16_t dc_offset;
    uint16_t peak;
    uint16_t rms;
    uint16_t envelope;
    uint16_t raw_min;
    uint16_t raw_max;
    uint16_t clipping_low;
    uint16_t clipping_high;
};

struct AudioLevelFrame {
    uint16_t left;
    uint16_t right;
    uint16_t aux;
    uint16_t mono;
    uint16_t left_peak;
    uint16_t right_peak;
    uint16_t aux_peak;
    bool left_clipping;
    bool right_clipping;
    bool aux_clipping;
    uint32_t sequence;
    uint32_t dropped_blocks;
    ChannelMetrics channels[kAudioChannels];
    ChannelMetrics mono_metrics;
};

struct AudioProcessor {
    int32_t dc_q8[kAudioChannels]{};
    uint16_t envelope[kAudioChannels]{};
    uint16_t mono_envelope = 0;
    bool initialized = false;
};

void audio_processor_reset(AudioProcessor& processor);

AudioLevelFrame process_audio_block(AudioProcessor& processor,
                                    const uint16_t* interleaved,
                                    uint32_t sequence,
                                    uint32_t dropped_blocks,
                                    CenteredMonoBlock* centered_mono_output = nullptr);
