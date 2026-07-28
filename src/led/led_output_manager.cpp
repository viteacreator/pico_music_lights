#include "led/led_output_manager.hpp"

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
