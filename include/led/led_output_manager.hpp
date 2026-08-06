#pragma once

#include <array>
#include <cstddef>

#include "board/led_board_config.hpp"
#include "led/led_status.hpp"
#include "led/led_strip.hpp"
#include "led/sk6812_rgbw_driver.hpp"

enum class FramePhase : uint8_t { idle, packing, transmitting, latching };

class LedOutputManager {
public:
  LedStatus
  configure(const std::array<LedStripConfig, board::kStripCount> &configs);
  LedStrip *strip(std::size_t index);
  const LedStrip *strip(std::size_t index) const;
  std::size_t configured_pixel_count() const;
  std::size_t usable_strip_count() const;
  LedStatus initialize_drivers();
  LedStatus
  ensure_drivers(const std::array<LedStripConfig, board::kStripCount> &configs);
  LedStatus start_show_one(std::size_t strip_index);
  LedStatus start_show_all_enabled();
  bool is_frame_in_progress() const;
  LedStatus poll_frame_completion();
  FramePhase frame_phase() const;

private:
  std::array<RgbwColor, board::kMaxConfiguredPixels> logical_pixel_pool_{};
  std::array<uint32_t, board::kMaxConfiguredPixels> packed_word_pool_{};
  std::array<LedStrip, board::kStripCount> strips_{};
  std::array<Sk6812RgbwDriver, board::kStripCount> drivers_{};
  std::size_t configured_pixel_count_ = 0;
  FramePhase phase_ = FramePhase::idle;
  uint32_t active_strip_mask_ = 0;
  uint64_t latch_deadline_us_ = 0;
  uint64_t frame_deadline_us_ = 0;

  LedStatus start_show(uint32_t requested_strip_mask);
  LedStatus timeout_frame();
};
