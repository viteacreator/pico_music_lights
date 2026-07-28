#pragma once
#include <cstddef>
#include <cstdint>
constexpr std::size_t kAudioChannels=3, kAudioSamplesPerChannel=256, kAudioInterleavedSamples=768;
struct ChannelMetrics { uint16_t dc_offset,peak,rms,envelope,raw_min,raw_max,clipping_low,clipping_high; };
struct AudioLevelFrame { uint16_t left,right,aux,mono,left_peak,right_peak,aux_peak; bool left_clipping,right_clipping,aux_clipping; uint32_t sequence,dropped_blocks; ChannelMetrics channels[3]; ChannelMetrics mono_metrics; };
struct AudioProcessor { int32_t dc_q8[3]{}; uint16_t envelope[3]{}; uint16_t mono_envelope = 0; bool initialized=false; };
void audio_processor_reset(AudioProcessor& processor);
AudioLevelFrame process_audio_block(AudioProcessor& processor,const uint16_t* interleaved,uint32_t sequence,uint32_t dropped_blocks);
