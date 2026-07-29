#include "audio/audio_processing.hpp"

#include <array>

namespace {

constexpr uint16_t kAdcMidpoint = 2048;

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
    const AudioLevelFrame in_phase = process_audio_block(processor, block.data(), 1, 0, &centered_mono);

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
           centered_mono.samples[1] == -100 && cancelling.mono_metrics.peak == 0 &&
           cancelling.mono_metrics.rms == 0;
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
    const uint16_t release_delta = static_cast<uint16_t>(rising.channels[0].envelope - falling.channels[0].envelope);
    return rising.channels[0].rms == 100 && falling.channels[0].rms == 0 &&
           attack_delta > release_delta;
}

}  // namespace

int main() {
    return test_audio_deinterleaving_and_offsets() &&
                   test_audio_peak_rms_mono_and_cancellation() &&
                   test_audio_dc_convergence_envelope_and_clipping() &&
                   test_audio_attack_is_faster_than_release()
               ? 0
               : 1;
}
