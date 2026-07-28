#include "led/sk6812_rgbw_driver.hpp"

#include <cstdio>

#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/regs/pio.h"
#include "pico/stdlib.h"
#include "sk6812_rgbw.pio.h"

namespace {

constexpr float kSk6812BitRateHz = 800000.0f;

struct ProgramState {
    bool loaded = false;
    uint offset = 0;
};

ProgramState g_pio0_program{};
ProgramState g_pio1_program{};

bool assignment_for_strip(uint8_t strip_index, PIO& pio, uint& state_machine) {
    if (strip_index < 4) {
        pio = pio0;
        state_machine = strip_index;
        return true;
    }
    if (strip_index == 4) {
        pio = pio1;
        state_machine = 0;
        return true;
    }
    return false;
}

LedStatus load_program(PIO pio, uint& offset) {
    ProgramState& state = (pio == pio0) ? g_pio0_program : g_pio1_program;
    if (!state.loaded) {
        if (!pio_can_add_program(pio, &sk6812_rgbw_program)) {
            return LedStatus::pio_program_load_failed;
        }
        state.offset = pio_add_program(pio, &sk6812_rgbw_program);
        state.loaded = true;
    }
    offset = state.offset;
    return LedStatus::ok;
}

}  // namespace

LedStatus Sk6812RgbwDriver::initialize(uint8_t strip_index, uint32_t gpio) {
    PIO pio = pio0;
    uint state_machine = 0;
    if (!assignment_for_strip(strip_index, pio, state_machine)) {
        return LedStatus::invalid_strip_index;
    }
    if (pio_sm_is_claimed(pio, state_machine)) {
        return LedStatus::pio_state_machine_unavailable;
    }

    uint offset = 0;
    const LedStatus program_status = load_program(pio, offset);
    if (program_status != LedStatus::ok) {
        return program_status;
    }

    pio_sm_claim(pio, state_machine);
    sk6812_rgbw_program_init(pio, state_machine, offset, gpio, kSk6812BitRateHz);
    pio_sm_set_enabled(pio, state_machine, true);

    initialized_ = true;
    strip_index_ = strip_index;
    gpio_ = gpio;
    pio_instance_ = pio;
    state_machine_ = static_cast<uint8_t>(state_machine);
    return LedStatus::ok;
}

bool Sk6812RgbwDriver::initialized() const { return initialized_; }
uint8_t Sk6812RgbwDriver::strip_index() const { return strip_index_; }
uint32_t Sk6812RgbwDriver::gpio() const { return gpio_; }
int Sk6812RgbwDriver::dma_channel() const { return dma_channel_; }

LedStatus Sk6812RgbwDriver::claim_dma_channel() {
    if (!initialized_) {
        return LedStatus::uninitialized;
    }
    if (dma_channel_ >= 0) {
        return LedStatus::ok;
    }

    const int claimed_channel = dma_claim_unused_channel(false);
    if (claimed_channel < 0) {
        return LedStatus::dma_channel_unavailable;
    }

    const PIO pio = static_cast<PIO>(pio_instance_);
    dma_channel_config config = dma_channel_get_default_config(claimed_channel);
    channel_config_set_transfer_data_size(&config, DMA_SIZE_32);
    channel_config_set_read_increment(&config, true);
    channel_config_set_write_increment(&config, false);
    dma_dreq_ = pio_get_dreq(pio, state_machine_, true);
    channel_config_set_dreq(&config, dma_dreq_);

    dma_channel_ = claimed_channel;
    std::printf("LED strip %u: PIO SM %u, GPIO %lu, DMA channel %d, DREQ %lu\n",
                static_cast<unsigned>(strip_index_), static_cast<unsigned>(state_machine_),
                static_cast<unsigned long>(gpio_), dma_channel_, static_cast<unsigned long>(dma_dreq_));
    return LedStatus::ok;
}

LedStatus Sk6812RgbwDriver::transmit_polling(const uint32_t* words, std::size_t word_count) {
    if (!initialized_) {
        return LedStatus::uninitialized;
    }
    if (words == nullptr || word_count == 0) {
        return LedStatus::invalid_pixel_count;
    }

    const PIO pio = static_cast<PIO>(pio_instance_);
    for (std::size_t index = 0; index < word_count; ++index) {
        pio_sm_put_blocking(pio, state_machine_, words[index]);
    }

    // The PIO sends one 32-bit word in approximately 40 us. This conservative
    // wait is only for the isolated polling bring-up path; Task 14 replaces it
    // with confirmed PIO completion before production latching.
    sleep_us(static_cast<uint32_t>(word_count * 45u + 100u));
    sleep_us(80u);
    return LedStatus::ok;
}

LedStatus Sk6812RgbwDriver::arm_dma(const uint32_t* words, std::size_t word_count) {
    if (!initialized_) return LedStatus::uninitialized;
    if (dma_channel_ < 0) return LedStatus::dma_channel_unavailable;
    if (words == nullptr || word_count == 0) return LedStatus::invalid_pixel_count;
    const PIO pio = static_cast<PIO>(pio_instance_);
    pio_sm_set_enabled(pio, state_machine_, false);
    pio_sm_clear_fifos(pio, state_machine_);
    pio_sm_restart(pio, state_machine_);
    const uint32_t stall_bit = 1u << (PIO_FDEBUG_TXSTALL_LSB + state_machine_);
    pio->fdebug = stall_bit;
    dma_channel_config config = dma_channel_get_default_config(dma_channel_);
    channel_config_set_transfer_data_size(&config, DMA_SIZE_32);
    channel_config_set_read_increment(&config, true);
    channel_config_set_write_increment(&config, false);
    channel_config_set_dreq(&config, dma_dreq_);
    dma_channel_configure(dma_channel_, &config, &pio->txf[state_machine_], words,
                          word_count, false);
    return LedStatus::ok;
}

bool Sk6812RgbwDriver::dma_complete() const {
    return dma_channel_ >= 0 && !dma_channel_is_busy(dma_channel_);
}

bool Sk6812RgbwDriver::physical_completion_confirmed() const {
    if (!initialized_ || !dma_complete()) return false;
    const PIO pio = static_cast<PIO>(pio_instance_);
    return (pio->fdebug & (1u << (PIO_FDEBUG_TXSTALL_LSB + state_machine_))) != 0;
}

void* Sk6812RgbwDriver::pio_instance() const { return pio_instance_; }
uint8_t Sk6812RgbwDriver::state_machine() const { return state_machine_; }
