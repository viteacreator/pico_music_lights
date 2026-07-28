#pragma once

#include <cstdint>

#include "led/channel_order.hpp"
#include "led/rgbw_color.hpp"

uint8_t scale_channel(uint8_t channel, uint8_t brightness);
RgbwColor scale_rgbw(const RgbwColor& color, uint8_t brightness);
uint32_t pack_rgbw(const RgbwColor& color, ChannelOrder order);
