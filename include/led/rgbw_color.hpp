#pragma once

#include <cstdint>

struct RgbwColor {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t white;
};

static_assert(sizeof(RgbwColor) == 4, "RgbwColor must occupy four bytes");
