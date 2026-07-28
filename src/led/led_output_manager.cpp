#include "led/led_output_manager.hpp"

#include "hardware/dma.h"
#include "hardware/pio.h"
#include "led/led_output_conversion.hpp"
#include "pico/stdlib.h"

LedStatus LedOutputManager::configure(const std::array<LedStripConfig, board::kStripCount>& configs) {
    std::size_t total = 0;
    for (const LedStripConfig& config : configs) {
        if (config.enabled && config.pixel_count == 0) {
            return LedStatus::invalid_pixel_count;
        }
        if (config.pixel_count > board::kMaxPixelsPerStrip) {
            return LedStatus::invalid_pixel_count;
        }
        total += config.pixel_count;
    }
    if (total > board::kMaxConfiguredPixels) {
        return LedStatus::excessive_total_pixel_count;
    }

    std::size_t offset = 0;
    for (std::size_t index = 0; index < configs.size(); ++index) {
        LedStatus status = strips_[index].configure(configs[index]);
        if (status != LedStatus::ok) {
            return status;
        }
        status = strips_[index].bind_slices(logical_pixel_pool_.data() + offset,
                                            packed_word_pool_.data() + offset,
                                            configs[index].pixel_count);
        if (status != LedStatus::ok) {
            return status;
        }
        offset += configs[index].pixel_count;
    }

    configured_pixel_count_ = total;
    return LedStatus::ok;
}

LedStrip* LedOutputManager::strip(std::size_t index) {
    return index < strips_.size() ? &strips_[index] : nullptr;
}

const LedStrip* LedOutputManager::strip(std::size_t index) const {
    return index < strips_.size() ? &strips_[index] : nullptr;
}

std::size_t LedOutputManager::configured_pixel_count() const { return configured_pixel_count_; }

LedStatus LedOutputManager::initialize_drivers() {
    std::size_t usable = 0;
    std::size_t failed = 0;
    for (std::size_t index = 0; index < strips_.size(); ++index) {
        if (!strips_[index].config().enabled) continue;
        LedStatus status = drivers_[index].initialize(static_cast<uint8_t>(index), strips_[index].config().gpio);
        if (status == LedStatus::ok) status = drivers_[index].claim_dma_channel();
        if (status == LedStatus::ok) ++usable; else ++failed;
    }
    if (usable == 0) return LedStatus::failed;
    return failed == 0 ? LedStatus::ok : LedStatus::partial_success;
}

LedStatus LedOutputManager::start_show_all_enabled() {
    if (phase_ != FramePhase::idle) return LedStatus::busy;
    phase_ = FramePhase::packing;
    uint32_t dma_mask = 0;
    uint32_t pio0_mask = 0;
    uint32_t pio1_mask = 0;
    for (std::size_t index = 0; index < strips_.size(); ++index) {
        if (!strips_[index].config().enabled || !drivers_[index].initialized()) continue;
        LedStatus status = pack_strip_for_output(strips_[index]);
        if (status != LedStatus::ok) { phase_ = FramePhase::idle; return status; }
        status = drivers_[index].arm_dma(strips_[index].packed_words(), strips_[index].pixel_count());
        if (status != LedStatus::ok) { phase_ = FramePhase::idle; return status; }
        dma_mask |= 1u << drivers_[index].dma_channel();
        if (static_cast<PIO>(drivers_[index].pio_instance()) == pio0)
            pio0_mask |= 1u << drivers_[index].state_machine();
        else pio1_mask |= 1u << drivers_[index].state_machine();
    }
    if (dma_mask == 0) { phase_ = FramePhase::idle; return LedStatus::failed; }
    dma_start_channel_mask(dma_mask);
    if (pio0_mask) pio_enable_sm_mask_in_sync(pio0, pio0_mask);
    if (pio1_mask) pio_set_sm_mask_enabled(pio1, pio1_mask, true);
    phase_ = FramePhase::transmitting;
    return LedStatus::ok;
}

bool LedOutputManager::is_frame_in_progress() const { return phase_ != FramePhase::idle; }
FramePhase LedOutputManager::frame_phase() const { return phase_; }

LedStatus LedOutputManager::poll_frame_completion() {
    if (phase_ == FramePhase::idle) return LedStatus::ok;
    if (phase_ == FramePhase::transmitting) {
        for (std::size_t index = 0; index < strips_.size(); ++index) {
            if (strips_[index].config().enabled && drivers_[index].initialized() &&
                !drivers_[index].physical_completion_confirmed()) return LedStatus::busy;
        }
        latch_deadline_us_ = time_us_64() + 80u;
        phase_ = FramePhase::latching;
        return LedStatus::busy;
    }
    if (phase_ == FramePhase::latching && time_us_64() >= latch_deadline_us_) {
        phase_ = FramePhase::idle;
        return LedStatus::ok;
    }
    return LedStatus::busy;
}
