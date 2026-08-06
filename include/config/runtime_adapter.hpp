#pragma once
#include "config/device_config.hpp"
#include "led/led_strip.hpp"
namespace config {
struct CurrentBoardRuntimeConfiguration {
  std::array<LedStripConfig, board::kStripCount> led{};
  std::array<effects::StripEffectConfig, effects::kEffectStripCount> effects{};
  effects::IdleLightingConfig idle{};
};
ValidationResult adapt_current_board(const DeviceConfiguration &,
                                     CurrentBoardRuntimeConfiguration &);
} // namespace config
