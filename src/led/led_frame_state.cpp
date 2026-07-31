#include "led/led_frame_state.hpp"

LedFramePollAction evaluate_led_transmission_poll(bool all_outputs_complete,
                                                  uint64_t now_us,
                                                  uint64_t deadline_us) {
    if (all_outputs_complete) {
        return LedFramePollAction::begin_latch;
    }

    return now_us >= deadline_us
               ? LedFramePollAction::timeout
               : LedFramePollAction::remain_transmitting;
}

bool led_latch_interval_complete(uint64_t now_us, uint64_t latch_deadline_us) {
    return now_us >= latch_deadline_us;
}
