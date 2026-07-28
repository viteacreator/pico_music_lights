#include <array>
#include <cstdio>

#include "board/led_board_config.hpp"
#include "led/led_output_manager.hpp"
#include "audio/audio_processing.hpp"
#include "pico/stdlib.h"

namespace {

constexpr std::array<uint16_t, board::kStripCount> kInstalledPixelCounts = {132, 174, 141, 81, 96, 72};
constexpr uint8_t kSafeBrightness = 16;

static_assert(sizeof(RgbwColor) == 4, "RGBW pixels must use four bytes");
static_assert(board::kStripCount == 6, "Expected six LED strips");
static_assert(board::kMaxPixelsPerStrip == 300, "Unexpected per-strip limit");
static_assert(board::kMaxConfiguredPixels == 800, "Unexpected total limit");

LedOutputManager g_manager;
bool g_ready = false;

bool initialize_hardware_test() {
    std::printf("LED RAM: logical pool %u B, packed pool %u B, total %u B\n",
                static_cast<unsigned>(board::kMaxConfiguredPixels * sizeof(RgbwColor)),
                static_cast<unsigned>(board::kMaxConfiguredPixels * sizeof(uint32_t)),
                static_cast<unsigned>(board::kMaxConfiguredPixels * (sizeof(RgbwColor) + sizeof(uint32_t))));
    std::printf("LED resources: PIO0 SM0-SM3, PIO1 SM0-SM1, up to six claimed DMA channels, max installed frame 7.04 ms\n");
    std::array<LedStripConfig, board::kStripCount> configs{};
    for (std::size_t index = 0; index < configs.size(); ++index) {
        configs[index] = {true, kInstalledPixelCounts[index], board::kStripGpios[index], kSafeBrightness,
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
    for (;;) {
        const LedStatus status = g_manager.poll_frame_completion();
        if (status == LedStatus::busy) {
            sleep_ms(1);
            continue;
        }
        if (status != LedStatus::ok) {
            std::printf("Frame completion failed: status %u\n", static_cast<unsigned>(status));
        }
        return;
    }
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

extern "C" bool led_vu_initialize(void) { return initialize_hardware_test(); }
extern "C" void led_vu_update(const AudioLevelFrame* frame) {
    static uint64_t last_update_us = 0;
    if (frame == nullptr || g_manager.is_frame_in_progress()) { g_manager.poll_frame_completion(); return; }
    if (time_us_64() - last_update_us < 16667u) return;
    last_update_us = time_us_64();
    const uint16_t levels[4] = {frame->left, frame->right, frame->aux, frame->mono};
    for (std::size_t index = 0; index < board::kStripCount; ++index) {
        LedStrip* strip = g_manager.strip(index); if (strip == nullptr) continue;
        const std::size_t lit = index < 4 ? (static_cast<std::size_t>(levels[index]) * strip->pixel_count() / 1024u) : 0;
        strip->fill({0, 0, 0, 0});
        for (std::size_t pixel = 0; pixel < lit && pixel < strip->pixel_count(); ++pixel) strip->set_pixel(pixel, {0, 255, 0, 0});
    }
    g_manager.start_show_all_enabled();
}
