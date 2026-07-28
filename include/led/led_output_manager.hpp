#pragma once

#include <array>
#include <cstddef>

#include "board/led_board_config.hpp"
#include "led/led_status.hpp"
#include "led/led_strip.hpp"

class LedOutputManager {
public:
    LedStatus configure(const std::array<LedStripConfig, board::kStripCount>& configs);
    LedStrip* strip(std::size_t index);
    const LedStrip* strip(std::size_t index) const;
    std::size_t configured_pixel_count() const;

private:
    std::array<RgbwColor, board::kMaxConfiguredPixels> logical_pixel_pool_{};
    std::array<uint32_t, board::kMaxConfiguredPixels> packed_word_pool_{};
    std::array<LedStrip, board::kStripCount> strips_{};
    std::size_t configured_pixel_count_ = 0;
};
