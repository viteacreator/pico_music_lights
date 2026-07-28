#include <array>
#include <cstdio>

#include "board/led_board_config.hpp"
#include "led/led_output_manager.hpp"
#include "pico/stdlib.h"

namespace {

constexpr uint16_t kTestPixels = 30;
constexpr uint8_t kSafeBrightness = 16;

static_assert(sizeof(RgbwColor) == 4, "RGBW pixels must use four bytes");
static_assert(board::kStripCount == 5, "Expected five LED strips");
static_assert(board::kMaxPixelsPerStrip == 300, "Unexpected per-strip limit");
static_assert(board::kMaxConfiguredPixels == 1200, "Unexpected total limit");

LedOutputManager g_manager;
bool g_ready = false;

bool initialize_hardware_test() {
    std::printf("LED RAM: logical pool %u B, packed pool %u B, total %u B\n",
                static_cast<unsigned>(board::kMaxConfiguredPixels * sizeof(RgbwColor)),
                static_cast<unsigned>(board::kMaxConfiguredPixels * sizeof(uint32_t)),
                static_cast<unsigned>(board::kMaxConfiguredPixels * (sizeof(RgbwColor) + sizeof(uint32_t))));
    std::printf("LED resources: PIO0 SM0-SM3, PIO1 SM0, up to five claimed DMA channels, max frame 12.08 ms\n");
    std::array<LedStripConfig, board::kStripCount> configs{};
    for (std::size_t index = 0; index < configs.size(); ++index) {
        configs[index] = {true, kTestPixels, board::kStripGpios[index], kSafeBrightness,
                          ChannelOrder::grbw, false};
        std::printf("Strip %u: GPIO %lu, pixels %u, order GRBW, brightness %u\n",
                    static_cast<unsigned>(index + 1),
                    static_cast<unsigned long>(configs[index].gpio),
                    static_cast<unsigned>(configs[index].pixel_count),
                    static_cast<unsigned>(configs[index].brightness));
    }
    if (g_manager.configure(configs) != LedStatus::ok) return false;
    const LedStatus result = g_manager.initialize_drivers();
    std::printf("LED initialization result: %u (0=ok, 1=partial success, 2=failed)\n",
                static_cast<unsigned>(result));
    return result == LedStatus::ok || result == LedStatus::partial_success;
}

void wait_for_frame() {
    while (g_manager.poll_frame_completion() == LedStatus::busy) sleep_ms(1);
}

void show_one(std::size_t selected_strip, const char* phase, RgbwColor color, uint32_t duration_ms) {
    std::printf("Phase: Strip %u %s\n", static_cast<unsigned>(selected_strip + 1), phase);
    for (std::size_t index = 0; index < board::kStripCount; ++index) {
        LedStrip* strip = g_manager.strip(index);
        if (strip != nullptr) strip->fill(index == selected_strip ? color : RgbwColor{0, 0, 0, 0});
    }
    const LedStatus status = g_manager.start_show_all_enabled();
    if (status != LedStatus::ok) std::printf("Frame start failed: status %u\n", static_cast<unsigned>(status));
    else wait_for_frame();
    sleep_ms(duration_ms);
}

}  // namespace

extern "C" void led_hardware_bringup_run_once(void) {
    if (!g_ready) {
        g_ready = initialize_hardware_test();
        if (!g_ready) {
            sleep_ms(1000);
            return;
        }
    }

    constexpr RgbwColor kOff{0, 0, 0, 0};
    constexpr RgbwColor kRed{255, 0, 0, 0};
    constexpr RgbwColor kGreen{0, 255, 0, 0};
    constexpr RgbwColor kBlue{0, 0, 255, 0};
    constexpr RgbwColor kWhite{0, 0, 0, 255};

    for (std::size_t strip = 0; strip < board::kStripCount; ++strip) {
        show_one(strip, "off", kOff, 250);
        show_one(strip, "red", kRed, 1000);
        show_one(strip, "off", kOff, 250);
        show_one(strip, "green", kGreen, 1000);
        show_one(strip, "off", kOff, 250);
        show_one(strip, "blue", kBlue, 1000);
        show_one(strip, "off", kOff, 250);
        show_one(strip, "neutral white", kWhite, 1000);
        show_one(strip, "off", kOff, 250);
    }
}
