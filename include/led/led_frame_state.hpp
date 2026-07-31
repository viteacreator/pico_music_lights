#pragma once

#include <cstdint>

enum class LedFramePollAction : uint8_t {
    remain_transmitting,
    begin_latch,
    timeout,
};

// Completion takes priority over deadline observation: a delayed normal-loop
// poll must accept a frame that PIO/DMA already finished physically.
LedFramePollAction evaluate_led_transmission_poll(bool all_outputs_complete,
                                                  uint64_t now_us,
                                                  uint64_t deadline_us);

bool led_latch_interval_complete(uint64_t now_us, uint64_t latch_deadline_us);
