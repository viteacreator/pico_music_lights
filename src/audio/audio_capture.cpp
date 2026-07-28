#include "audio/audio_capture.hpp"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/sync.h"

namespace {
enum class BufferState : uint8_t { free, filling, ready, processing };
uint16_t buffers[2][kAudioInterleavedSamples];
volatile BufferState states[2] = {BufferState::free, BufferState::free};
volatile int filling = -1;
volatile uint32_t dropped = 0;
int dma_channel = -1;
AudioProcessor processor;
uint32_t overflow_events = 0;
uint32_t underflow_events = 0;

void start_fill(int index) {
    states[index] = BufferState::filling;
    filling = index;
    dma_channel_set_write_addr(dma_channel, buffers[index], false);
    dma_channel_set_trans_count(dma_channel, kAudioInterleavedSamples, true);
}

void dma_isr() {
    dma_hw->ints0 = 1u << dma_channel;
    const int completed = filling;
    const int next = completed ^ 1;
    if (states[next] == BufferState::free) { states[completed] = BufferState::ready; start_fill(next); }
    else { ++dropped; states[completed] = BufferState::free; start_fill(completed); }
}

void clear_fifo_errors() {
    while (adc_fifo_get_level() != 0) (void)adc_fifo_get();
    adc_hw->fcs = ADC_FCS_OVER_BITS | ADC_FCS_UNDER_BITS;
}

void record_and_clear_fifo_errors() {
    const uint32_t errors = adc_hw->fcs & (ADC_FCS_OVER_BITS | ADC_FCS_UNDER_BITS);
    if (errors & ADC_FCS_OVER_BITS) ++overflow_events;
    if (errors & ADC_FCS_UNDER_BITS) ++underflow_events;
    if (errors != 0) adc_hw->fcs = adc_hw->fcs | errors;
}
}

bool audio_capture_initialize() {
    adc_init(); adc_run(false); clear_fifo_errors();
    for (uint gpio = 26; gpio <= 28; ++gpio) adc_gpio_init(gpio);
    adc_select_input(0); adc_set_round_robin(0x7); adc_fifo_setup(true, true, 1, false, false); adc_set_clkdiv(499.0f);
    dma_channel = dma_claim_unused_channel(false); if (dma_channel < 0) return false;
    dma_channel_config config = dma_channel_get_default_config(dma_channel);
    channel_config_set_transfer_data_size(&config, DMA_SIZE_16); channel_config_set_read_increment(&config, false);
    channel_config_set_write_increment(&config, true); channel_config_set_dreq(&config, DREQ_ADC);
    dma_channel_configure(dma_channel, &config, buffers[0], &adc_hw->fifo, kAudioInterleavedSamples, false);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_isr); dma_channel_set_irq0_enabled(dma_channel, true); irq_set_enabled(DMA_IRQ_0, true);
    states[0] = BufferState::filling; filling = 0; dma_channel_start(dma_channel); adc_run(true);
    return true;
}

bool audio_capture_process(AudioLevelFrame& frame) {
    int index = -1; const uint32_t irq = save_and_disable_interrupts();
    for (int i = 0; i < 2; ++i) if (states[i] == BufferState::ready) { states[i] = BufferState::processing; index = i; break; }
    const uint32_t local_dropped = dropped; restore_interrupts(irq);
    if (index < 0) return false;
    frame = process_audio_block(processor, buffers[index], frame.sequence + 1, local_dropped);
    const uint32_t lock = save_and_disable_interrupts(); states[index] = BufferState::free; if (filling < 0) start_fill(index); restore_interrupts(lock);
    record_and_clear_fifo_errors();
    return true;
}
uint32_t audio_capture_dma_channel() { return dma_channel < 0 ? 0xffffffffu : static_cast<uint32_t>(dma_channel); }
uint32_t audio_capture_fifo_errors() { return overflow_events; }
uint32_t audio_capture_fifo_underflows() { return underflow_events; }
