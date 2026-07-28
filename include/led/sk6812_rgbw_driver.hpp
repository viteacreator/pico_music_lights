#pragma once

#include <cstddef>
#include <cstdint>

#include "led/led_status.hpp"

class Sk6812RgbwDriver {
public:
    LedStatus initialize(uint8_t strip_index, uint32_t gpio);
    bool initialized() const;
    uint8_t strip_index() const;
    uint32_t gpio() const;
    int dma_channel() const;
    LedStatus claim_dma_channel();
    LedStatus transmit_polling(const uint32_t* words, std::size_t word_count);

private:
    bool initialized_ = false;
    uint8_t strip_index_ = 0;
    uint32_t gpio_ = 0;
    int dma_channel_ = -1;
    uint32_t dma_dreq_ = 0;
    void* pio_instance_ = nullptr;
    uint8_t state_machine_ = 0;
};
