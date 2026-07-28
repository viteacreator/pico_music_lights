#include "led/channel_order.hpp"
#include "led/led_status.hpp"
#include "led/led_strip.hpp"
#include "led/led_output_conversion.hpp"
#include "led/led_output_manager.hpp"
#include "led/rgbw_color.hpp"
#include "led/rgbw_conversion.hpp"

int main() {
    const RgbwColor color{1, 2, 3, 4};
    const bool valid_statuses = InitializationResult::ok != InitializationResult::failed &&
                                InitializationResult::partial_success != InitializationResult::failed &&
                                LedStatus::busy != LedStatus::ok;
    const RgbwColor scaled = scale_rgbw({255, 128, 1, 64}, 128);
    const bool conversion_ok =
        pack_rgbw({0x11, 0x22, 0x33, 0x44}, ChannelOrder::rgbw) == 0x11223344u &&
        pack_rgbw({0x11, 0x22, 0x33, 0x44}, ChannelOrder::grbw) == 0x22113344u &&
        scale_channel(0, 127) == 0 && scale_channel(200, 255) == 200 &&
        scaled.red == 128 && scaled.green == 64 && scaled.blue == 1 && scaled.white == 32;
    LedStrip strip;
    RgbwColor logical[2]{};
    uint32_t packed[2]{};
    const bool validation_ok =
        strip.configure({true, 0, 2, 32, ChannelOrder::rgbw, false}) == LedStatus::invalid_pixel_count &&
        strip.configure({true, 301, 2, 32, ChannelOrder::rgbw, false}) == LedStatus::invalid_pixel_count &&
        strip.configure({true, 2, 2, 32, ChannelOrder::rgbw, true}) == LedStatus::ok &&
        strip.bind_slices(logical, packed, 2) == LedStatus::ok &&
        strip.set_pixel(0, {1, 2, 3, 4}) == LedStatus::ok &&
        strip.set_pixel(1, {5, 6, 7, 8}) == LedStatus::ok;
    RgbwColor first{};
    const bool stable_indexing_ok = strip.get_pixel(0, first) == LedStatus::ok && first.red == 1 &&
                                    strip.get_pixel(2, first) == LedStatus::invalid_pixel_index &&
                                    strip.clear() == LedStatus::ok && logical[0].red == 0 && logical[1].white == 0;
    logical[0] = {10, 0, 0, 0};
    logical[1] = {20, 0, 0, 0};
    const bool reversed_output_ok = pack_strip_for_output(strip) == LedStatus::ok &&
                                    packed[0] == 0x14000000u && packed[1] == 0x0A000000u &&
                                    logical[0].red == 10 && logical[1].red == 20;
    LedOutputManager manager;
    std::array<LedStripConfig, board::kStripCount> manager_configs{};
    manager_configs[0] = {true, 300, 2, 32, ChannelOrder::rgbw, false};
    manager_configs[1] = {true, 300, 3, 32, ChannelOrder::rgbw, false};
    manager_configs[2] = {true, 201, 4, 32, ChannelOrder::rgbw, false};
    const bool pool_validation_ok =
        manager.configure(manager_configs) == LedStatus::excessive_total_pixel_count;
    return (sizeof(color) == 4 && ChannelOrder::rgbw != ChannelOrder::grbw && valid_statuses &&
            conversion_ok && validation_ok && stable_indexing_ok && reversed_output_ok && pool_validation_ok)
               ? 0
               : 1;
}
