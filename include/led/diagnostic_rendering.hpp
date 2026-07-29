#pragma once

#include "audio/spectrum_analyzer.hpp"
#include "led/rgbw_color.hpp"

#include <cstddef>
#include <cstdint>

struct LogicalRgbwPixels {
    RgbwColor* data = nullptr;
    std::size_t size = 0;
};

// Every function operates only on the caller-owned logical pixel span. Invalid
// spans are ignored. No function accesses a LedStrip, PIO, DMA, or GPIO.
void clear_logical_pixels(LogicalRgbwPixels pixels);

// Feature 002 envelopes use centred ADC-amplitude units (approximately
// 0..2047). Convert them to the full 0..65535 diagnostic display range before
// applying VU geometry. Values above the expected range saturate.
uint16_t normalize_audio_envelope_for_diagnostic(uint16_t envelope);

// Feature 003 spectrum levels already use 0..65535. This provisional square
// root display curve keeps the range bounded while making low, valid spectral
// levels visible at the deliberately low hardware brightness.
uint16_t normalize_spectrum_level_for_diagnostic(uint16_t level);

// Resamples 32 spectrum bands to 16 contiguous physical segments, stretched
// across the supplied span. The source contribution weighting is delegated to
// the shared spectrum resampler and levels use the diagnostic spectrum curve.
// Colour moves from low-frequency blue/white through green to high-frequency
// red.
void render_spectrum_32_to_16(LogicalRgbwPixels pixels,
                              const SpectrumFrame& spectrum);

// Maps the 32 spectrum bands to five contiguous frequency zones and mirrors
// the five zone levels around the centre of the supplied span.
void render_symmetric_frequency_zones(LogicalRgbwPixels pixels,
                                      const SpectrumFrame& spectrum);

// Renders contiguous Bass, Mid, and High zones in red, green, and blue.
void render_bass_mid_high_zones(LogicalRgbwPixels pixels,
                                const SpectrumFrame& spectrum);

// Renders Feature 002 left and right envelope levels from the centre outward.
// On an odd span the centre pixel belongs to Left and Right begins immediately
// to its right.
void render_stereo_center_out(LogicalRgbwPixels pixels,
                              uint16_t left_level,
                              uint16_t right_level,
                              RgbwColor left_color,
                              RgbwColor right_color);

// Renders one Feature 002 envelope VU level from the beginning of the
// caller-provided span.
void render_full_vu(LogicalRgbwPixels pixels,
                    uint16_t level,
                    RgbwColor color);

// Pure scheduling predicate for the runtime renderer. It permits no request
// while disabled or while the LED manager owns the logical buffers.
bool diagnostic_frame_should_start(bool renderer_enabled,
                                   bool frame_in_progress,
                                   uint64_t now_us,
                                   uint64_t last_update_us,
                                   uint64_t interval_us);

// Pure startup policy: the temporary diagnostic scene starts automatically
// only when the optional LED runtime has at least one usable strip.
bool diagnostic_renderer_should_auto_enable(bool runtime_available,
                                            std::size_t usable_strip_count);
