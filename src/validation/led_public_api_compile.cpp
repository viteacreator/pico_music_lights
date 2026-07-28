#include "led/channel_order.hpp"
#include "led/led_status.hpp"
#include "led/rgbw_color.hpp"
#include "board/led_board_config.hpp"

static_assert(sizeof(RgbwColor) == 4, "Public RGBW representation changed");

[[maybe_unused]] constexpr ChannelOrder kCompileValidationOrder = ChannelOrder::rgbw;
static_assert(board::kStripCount == 5, "Expected five physical LED strips");

[[maybe_unused]] constexpr InitializationResult kCompileValidationResult =
    InitializationResult::partial_success;
