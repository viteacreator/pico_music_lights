#include "config/runtime_adapter.hpp"
namespace config {
ValidationResult adapt_current_board(const DeviceConfiguration &value,
                                     CurrentBoardRuntimeConfiguration &output) {
  const ValidationResult result = validate(value);
  if (!result) {
    return result;
  }
  CurrentBoardRuntimeConfiguration next{};
  for (std::size_t index = 0; index < board::kStripCount; ++index) {
    const auto &source = value.led_channels[index];
    const ::ChannelOrder runtime_order =
        source.channel_order == config::ChannelOrder::grbw
            ? ::ChannelOrder::grbw
            : ::ChannelOrder::rgbw;
    next.led[index] = {source.enabled,
                       source.enabled ? source.pixel_count : uint16_t{0},
                       board::kStripGpios[index],
                       source.brightness,
                       runtime_order,
                       source.reversed};
    next.effects[index] =
        to_runtime(value.effects[index], value.audio_calibration);
  }
  next.idle = to_runtime(value.idle_lighting);
  output = next;
  return {};
}
} // namespace config
