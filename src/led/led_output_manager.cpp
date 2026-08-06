#include "led/led_output_manager.hpp"

#include <algorithm>
#include <cstdio>

#include "hardware/dma.h"
#include "hardware/pio.h"
#include "led/led_frame_state.hpp"
#include "led/led_output_conversion.hpp"
#include "pico/stdlib.h"

namespace {

constexpr uint64_t kSk6812PixelTimeUs = 40u;
constexpr uint64_t kLatchIntervalUs = 80u;
constexpr uint64_t kFrameTimeoutMarginUs = 2000u;

} // namespace

LedStatus LedOutputManager::configure(
    const std::array<LedStripConfig, board::kStripCount> &configs) {
  if (phase_ != FramePhase::idle)
    return LedStatus::busy;

  std::size_t total = 0;
  std::array<bool, board::kStripCount> gpio_used{};
  for (std::size_t index = 0; index < configs.size(); ++index) {
    const LedStripConfig &config = configs[index];
    if (!is_supported_channel_order(config.channel_order))
      return LedStatus::unsupported_channel_order;
    if (config.enabled && config.pixel_count == 0)
      return LedStatus::invalid_pixel_count;
    if (config.pixel_count > board::kMaxPixelsPerStrip)
      return LedStatus::invalid_pixel_count;
    if (config.enabled && config.gpio != board::kStripGpios[index]) {
      return LedStatus::invalid_gpio_assignment;
    }
    if (config.enabled) {
      for (std::size_t earlier = 0; earlier < index; ++earlier) {
        if (gpio_used[earlier] && configs[earlier].gpio == config.gpio) {
          return LedStatus::invalid_gpio_assignment;
        }
      }
      gpio_used[index] = true;
    }
    total += config.pixel_count;
  }
  if (total > board::kMaxConfiguredPixels)
    return LedStatus::excessive_total_pixel_count;

  // All validation has succeeded; only now replace the existing strip slices.
  std::size_t offset = 0;
  for (std::size_t index = 0; index < configs.size(); ++index) {
    LedStatus status = strips_[index].configure(configs[index]);
    if (status != LedStatus::ok)
      return status;
    status = strips_[index].bind_slices(logical_pixel_pool_.data() + offset,
                                        packed_word_pool_.data() + offset,
                                        configs[index].pixel_count);
    if (status != LedStatus::ok)
      return status;
    offset += configs[index].pixel_count;
  }
  configured_pixel_count_ = total;
  active_strip_mask_ = 0;
  return LedStatus::ok;
}

LedStrip *LedOutputManager::strip(std::size_t index) {
  return index < strips_.size() ? &strips_[index] : nullptr;
}

const LedStrip *LedOutputManager::strip(std::size_t index) const {
  return index < strips_.size() ? &strips_[index] : nullptr;
}

std::size_t LedOutputManager::configured_pixel_count() const {
  return configured_pixel_count_;
}

std::size_t LedOutputManager::usable_strip_count() const {
  std::size_t count = 0;

  for (const Sk6812RgbwDriver &driver : drivers_) {
    if (driver.usable()) {
      ++count;
    }
  }

  return count;
}

LedStatus LedOutputManager::ensure_drivers(
    const std::array<LedStripConfig, board::kStripCount> &configs) {
  for (std::size_t index = 0; index < configs.size(); ++index) {
    if (!configs[index].enabled) {
      continue;
    }
    LedStatus status = drivers_[index].initialize(static_cast<uint8_t>(index),
                                                  board::kStripGpios[index]);
    if (status == LedStatus::ok) {
      status = drivers_[index].claim_dma_channel();
    }
    if (!drivers_[index].usable()) {
      return status;
    }
  }
  return LedStatus::ok;
}

LedStatus LedOutputManager::initialize_drivers() {
  std::array<LedStripConfig, board::kStripCount> configs{};
  for (std::size_t index = 0; index < configs.size(); ++index) {
    configs[index] = strips_[index].config();
  }
  const LedStatus status = ensure_drivers(configs);
  if (status != LedStatus::ok) {
    std::size_t usable = 0u;
    for (const auto &driver : drivers_) {
      usable += driver.usable() ? 1u : 0u;
    }
    return usable == 0u ? LedStatus::failed : LedStatus::partial_success;
  }
  return LedStatus::ok;
}

LedStatus LedOutputManager::start_show_one(std::size_t strip_index) {
  if (strip_index >= strips_.size())
    return LedStatus::invalid_strip_index;
  return start_show(1u << strip_index);
}

LedStatus LedOutputManager::start_show_all_enabled() {
  return start_show((1u << board::kStripCount) - 1u);
}

LedStatus LedOutputManager::start_show(uint32_t requested_strip_mask) {
  if (phase_ != FramePhase::idle)
    return LedStatus::busy;
  phase_ = FramePhase::packing;
  active_strip_mask_ = 0;
  uint32_t dma_mask = 0;
  uint32_t pio0_mask = 0;
  uint32_t pio1_mask = 0;
  uint16_t longest_pixel_count = 0;

  for (std::size_t index = 0; index < strips_.size(); ++index) {
    const uint32_t strip_mask = 1u << index;
    if ((requested_strip_mask & strip_mask) == 0 ||
        !strips_[index].config().enabled || !drivers_[index].usable()) {
      continue;
    }
    LedStatus status = pack_strip_for_output(strips_[index]);
    if (status != LedStatus::ok) {
      phase_ = FramePhase::idle;
      return status;
    }
    status = drivers_[index].arm_dma(strips_[index].packed_words(),
                                     strips_[index].pixel_count());
    if (status != LedStatus::ok) {
      for (std::size_t active = 0; active < strips_.size(); ++active) {
        if (active_strip_mask_ & (1u << active))
          drivers_[active].abort_transmission();
      }
      active_strip_mask_ = 0;
      phase_ = FramePhase::idle;
      return status;
    }
    active_strip_mask_ |= strip_mask;
    longest_pixel_count = std::max<uint16_t>(
        longest_pixel_count, strips_[index].config().pixel_count);
    dma_mask |= 1u << drivers_[index].dma_channel();
    if (static_cast<PIO>(drivers_[index].pio_instance()) == pio0) {
      pio0_mask |= 1u << drivers_[index].state_machine();
    } else {
      pio1_mask |= 1u << drivers_[index].state_machine();
    }
  }

  if (active_strip_mask_ == 0) {
    phase_ = FramePhase::idle;
    return LedStatus::failed;
  }

  frame_deadline_us_ = time_us_64() + longest_pixel_count * kSk6812PixelTimeUs +
                       kLatchIntervalUs + kFrameTimeoutMarginUs;
  dma_start_channel_mask(dma_mask);

  // PIO is still disabled. Preload each FIFO before clearing TXSTALL so a
  // pre-frame blocking pull can never be mistaken for final-word completion.
  for (;;) {
    bool all_preloaded = true;
    for (std::size_t index = 0; index < strips_.size(); ++index) {
      if ((active_strip_mask_ & (1u << index)) &&
          !drivers_[index].tx_fifo_has_word()) {
        all_preloaded = false;
        break;
      }
    }
    if (all_preloaded)
      break;
    if (time_us_64() >= frame_deadline_us_)
      return timeout_frame();
  }
  for (std::size_t index = 0; index < strips_.size(); ++index) {
    if (active_strip_mask_ & (1u << index))
      drivers_[index].clear_tx_stall();
  }
  if (pio0_mask)
    pio_enable_sm_mask_in_sync(pio0, pio0_mask);
  if (pio1_mask)
    pio_enable_sm_mask_in_sync(pio1, pio1_mask);
  phase_ = FramePhase::transmitting;
  return LedStatus::ok;
}

bool LedOutputManager::is_frame_in_progress() const {
  return phase_ != FramePhase::idle;
}
FramePhase LedOutputManager::frame_phase() const { return phase_; }

LedStatus LedOutputManager::timeout_frame() {
  for (std::size_t index = 0; index < strips_.size(); ++index) {
    if ((active_strip_mask_ & (1u << index)) != 0u) {
      drivers_[index].abort_transmission();
    }
  }
  active_strip_mask_ = 0;
  phase_ = FramePhase::idle;
  return LedStatus::transmission_timeout;
}

LedStatus LedOutputManager::poll_frame_completion() {
  if (phase_ == FramePhase::idle)
    return LedStatus::ok;

  if (phase_ == FramePhase::transmitting) {
    bool all_outputs_complete = true;

    for (std::size_t index = 0; index < strips_.size(); ++index) {
      if ((active_strip_mask_ & (1u << index)) &&
          !drivers_[index].physical_completion_confirmed()) {
        all_outputs_complete = false;
        break;
      }
    }

    switch (evaluate_led_transmission_poll(all_outputs_complete, time_us_64(),
                                           frame_deadline_us_)) {
    case LedFramePollAction::begin_latch:
      latch_deadline_us_ = time_us_64() + kLatchIntervalUs;
      phase_ = FramePhase::latching;
      return LedStatus::busy;
    case LedFramePollAction::timeout:
      return timeout_frame();
    case LedFramePollAction::remain_transmitting:
      return LedStatus::busy;
    }
  }

  if (phase_ == FramePhase::latching &&
      led_latch_interval_complete(time_us_64(), latch_deadline_us_)) {
    active_strip_mask_ = 0;
    phase_ = FramePhase::idle;
    return LedStatus::ok;
  }
  return LedStatus::busy;
}
