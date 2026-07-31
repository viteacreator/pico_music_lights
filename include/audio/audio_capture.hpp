#pragma once

#include "audio/audio_processing.hpp"

bool audio_capture_initialize();
bool audio_capture_has_ready_block();
bool audio_capture_process(AudioLevelFrame& frame, CenteredMonoBlock& centered_mono);
uint32_t audio_capture_dma_channel();
uint32_t audio_capture_fifo_errors();
uint32_t audio_capture_fifo_underflows();
