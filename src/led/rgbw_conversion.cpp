#include "led/rgbw_conversion.hpp"

uint8_t scale_channel(uint8_t channel, uint8_t brightness) {
    return static_cast<uint8_t>(
        (static_cast<uint16_t>(channel) * brightness + 127u) / 255u);
}

RgbwColor scale_rgbw(const RgbwColor& color, uint8_t brightness) {
    return {
        scale_channel(color.red, brightness),
        scale_channel(color.green, brightness),
        scale_channel(color.blue, brightness),
        scale_channel(color.white, brightness),
    };
}

uint32_t pack_rgbw(const RgbwColor& color, ChannelOrder order) {
    switch (order) {
        case ChannelOrder::rgbw:
            return (static_cast<uint32_t>(color.red) << 24u) |
                   (static_cast<uint32_t>(color.green) << 16u) |
                   (static_cast<uint32_t>(color.blue) << 8u) |
                   static_cast<uint32_t>(color.white);
        case ChannelOrder::grbw:
            return (static_cast<uint32_t>(color.green) << 24u) |
                   (static_cast<uint32_t>(color.red) << 16u) |
                   (static_cast<uint32_t>(color.blue) << 8u) |
                   static_cast<uint32_t>(color.white);
    }

    return 0;
}
