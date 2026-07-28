#include <array>
#include <cstdio>

#include "led/rgbw_conversion.hpp"
#include "led/sk6812_rgbw_driver.hpp"
#include "pico/stdlib.h"

namespace {

constexpr uint32_t kBringupGpio = 2;
constexpr std::size_t kBringupPixels = 30;
constexpr uint8_t kSafeBrightness = 16;
constexpr ChannelOrder kBringupOrder = ChannelOrder::grbw;

Sk6812RgbwDriver g_driver;
std::array<uint32_t, kBringupPixels> g_words{};
bool g_ready = false;

bool initialize_bringup() {
    std::printf("SK6812 RGBW bring-up: GPIO %lu, pixels %u, order GRBW, brightness %u\n",
                static_cast<unsigned long>(kBringupGpio), static_cast<unsigned>(kBringupPixels),
                static_cast<unsigned>(kSafeBrightness));
    LedStatus status = g_driver.initialize(0, kBringupGpio);
    if (status != LedStatus::ok) {
        std::printf("LED initialization failed: status %u\n", static_cast<unsigned>(status));
        return false;
    }
    status = g_driver.claim_dma_channel();
    if (status != LedStatus::ok) {
        std::printf("DMA initialization failed: status %u\n", static_cast<unsigned>(status));
        return false;
    }
    return true;
}

void show_phase(const char* phase, const RgbwColor& logical_color, uint32_t duration_ms) {
    std::printf("Phase: %s\n", phase);
    const uint32_t word = pack_rgbw(scale_rgbw(logical_color, kSafeBrightness), kBringupOrder);
    g_words.fill(word);
    const LedStatus status = g_driver.transmit_polling(g_words.data(), g_words.size());
    if (status != LedStatus::ok) {
        std::printf("Transmission failed: status %u\n", static_cast<unsigned>(status));
    }
    sleep_ms(duration_ms);
}

}  // namespace

extern "C" void led_hardware_bringup_run_once(void) {
    if (!g_ready) {
        g_ready = initialize_bringup();
        if (!g_ready) {
            sleep_ms(1000);
            return;
        }
    }

    constexpr RgbwColor kOff{0, 0, 0, 0};
    show_phase("all pixels off", kOff, 250);
    show_phase("low red", {255, 0, 0, 0}, 1000);
    show_phase("off", kOff, 250);
    show_phase("low green", {0, 255, 0, 0}, 1000);
    show_phase("off", kOff, 250);
    show_phase("low blue", {0, 0, 255, 0}, 1000);
    show_phase("off", kOff, 250);
    show_phase("low dedicated white", {0, 0, 0, 255}, 1000);
    show_phase("off", kOff, 250);
}
