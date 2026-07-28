#pragma once

#include <cstdint>

enum class LedStatus : uint8_t {
    ok,
    partial_success,
    failed,
    busy,
    invalid_strip_index,
    invalid_pixel_index,
    invalid_pixel_count,
    excessive_total_pixel_count,
    unsupported_channel_order,
    uninitialized,
    pio_program_load_failed,
    pio_state_machine_unavailable,
    dma_channel_unavailable,
    transmission_timeout,
    invalid_gpio_assignment,
};

enum class InitializationResult : uint8_t {
    ok,
    partial_success,
    failed,
};
