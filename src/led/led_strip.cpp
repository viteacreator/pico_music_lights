#include "led/led_strip.hpp"

#include "board/led_board_config.hpp"

bool is_supported_channel_order(ChannelOrder order) {
    return order == ChannelOrder::rgbw || order == ChannelOrder::grbw;
}

LedStatus LedStrip::configure(const LedStripConfig& config) {
    if (!is_supported_channel_order(config.channel_order)) {
        return LedStatus::unsupported_channel_order;
    }
    if (config.enabled && config.pixel_count == 0) {
        return LedStatus::invalid_pixel_count;
    }
    if (config.pixel_count > board::kMaxPixelsPerStrip) {
        return LedStatus::invalid_pixel_count;
    }

    config_ = config;
    logical_pixels_ = nullptr;
    packed_words_ = nullptr;
    capacity_ = 0;
    configured_ = true;
    return LedStatus::ok;
}

LedStatus LedStrip::bind_slices(RgbwColor* logical_pixels, uint32_t* packed_words,
                                std::size_t capacity) {
    if (!configured_) {
        return LedStatus::uninitialized;
    }
    if (config_.pixel_count > capacity ||
        (config_.pixel_count != 0 && (logical_pixels == nullptr || packed_words == nullptr))) {
        return LedStatus::invalid_pixel_count;
    }

    logical_pixels_ = logical_pixels;
    packed_words_ = packed_words;
    capacity_ = capacity;
    return LedStatus::ok;
}

LedStatus LedStrip::set_pixel(std::size_t index, const RgbwColor& color) {
    if (!configured_ || logical_pixels_ == nullptr) {
        return LedStatus::uninitialized;
    }
    if (index >= config_.pixel_count) {
        return LedStatus::invalid_pixel_index;
    }
    logical_pixels_[index] = color;
    return LedStatus::ok;
}

LedStatus LedStrip::get_pixel(std::size_t index, RgbwColor& color) const {
    if (!configured_ || logical_pixels_ == nullptr) {
        return LedStatus::uninitialized;
    }
    if (index >= config_.pixel_count) {
        return LedStatus::invalid_pixel_index;
    }
    color = logical_pixels_[index];
    return LedStatus::ok;
}

LedStatus LedStrip::fill(const RgbwColor& color) {
    if (!configured_ || logical_pixels_ == nullptr) {
        return LedStatus::uninitialized;
    }
    for (std::size_t index = 0; index < config_.pixel_count; ++index) {
        logical_pixels_[index] = color;
    }
    return LedStatus::ok;
}

LedStatus LedStrip::clear() { return fill({0, 0, 0, 0}); }

const LedStripConfig& LedStrip::config() const { return config_; }
bool LedStrip::is_configured() const { return configured_; }
std::size_t LedStrip::pixel_count() const { return config_.pixel_count; }
RgbwColor* LedStrip::logical_pixels() { return logical_pixels_; }
const RgbwColor* LedStrip::logical_pixels() const { return logical_pixels_; }
uint32_t* LedStrip::packed_words() { return packed_words_; }
const uint32_t* LedStrip::packed_words() const { return packed_words_; }
