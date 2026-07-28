#pragma once

#include <cstddef>
#include <cstdint>

#include "led/channel_order.hpp"
#include "led/led_status.hpp"
#include "led/rgbw_color.hpp"

struct LedStripConfig {
    bool enabled = false;
    uint16_t pixel_count = 0;
    uint32_t gpio = 0;
    uint8_t brightness = 0;
    ChannelOrder channel_order = ChannelOrder::rgbw;
    bool reversed = false;
};

bool is_supported_channel_order(ChannelOrder order);

class LedStrip {
public:
    LedStatus configure(const LedStripConfig& config);
    LedStatus bind_slices(RgbwColor* logical_pixels, uint32_t* packed_words, std::size_t capacity);
    LedStatus set_pixel(std::size_t index, const RgbwColor& color);
    LedStatus get_pixel(std::size_t index, RgbwColor& color) const;
    LedStatus fill(const RgbwColor& color);
    LedStatus clear();

    const LedStripConfig& config() const;
    bool is_configured() const;
    std::size_t pixel_count() const;
    RgbwColor* logical_pixels();
    const RgbwColor* logical_pixels() const;
    uint32_t* packed_words();
    const uint32_t* packed_words() const;

private:
    LedStripConfig config_{};
    RgbwColor* logical_pixels_ = nullptr;
    uint32_t* packed_words_ = nullptr;
    std::size_t capacity_ = 0;
    bool configured_ = false;
};
