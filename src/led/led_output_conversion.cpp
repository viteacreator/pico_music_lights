#include "led/led_output_conversion.hpp"

#include "led/rgbw_conversion.hpp"

LedStatus pack_strip_for_output(LedStrip& strip) {
    if (!strip.is_configured() || strip.logical_pixels() == nullptr || strip.packed_words() == nullptr) {
        return LedStatus::uninitialized;
    }

    const std::size_t count = strip.pixel_count();
    const LedStripConfig& config = strip.config();
    const RgbwColor* logical_pixels = strip.logical_pixels();
    uint32_t* packed_words = strip.packed_words();

    for (std::size_t output_index = 0; output_index < count; ++output_index) {
        const std::size_t logical_index = config.reversed ? (count - 1u - output_index) : output_index;
        packed_words[output_index] =
            pack_rgbw(scale_rgbw(logical_pixels[logical_index], config.brightness), config.channel_order);
    }
    return LedStatus::ok;
}
