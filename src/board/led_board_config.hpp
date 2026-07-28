#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace board {

constexpr std::size_t kStripCount = 5;
constexpr uint16_t kMaxPixelsPerStrip = 300;
constexpr uint16_t kMaxConfiguredPixels = 1200;
constexpr uint16_t kDefaultPixelsPerMetre = 60;

constexpr std::array<uint32_t, kStripCount> kStripGpios = {2, 3, 4, 5, 6};

constexpr bool strip_gpios_are_unique() {
    for (std::size_t first = 0; first < kStripGpios.size(); ++first) {
        for (std::size_t second = first + 1; second < kStripGpios.size(); ++second) {
            if (kStripGpios[first] == kStripGpios[second]) {
                return false;
            }
        }
    }
    return true;
}

static_assert(strip_gpios_are_unique(), "LED GPIO assignments must be unique");

}  // namespace board
