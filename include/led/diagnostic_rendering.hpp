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

// Resamples 32 spectrum bands to 16 contiguous physical segments, stretched
// across the supplied span. The source contribution weighting is delegated to
// the shared spectrum resampler. Colour moves from low-frequency blue/white
// through green to high-frequency red.
void render_spectrum_32_to_16(LogicalRgbwPixels pixels,
                              const SpectrumFrame& spectrum);

// Maps the 32 spectrum bands to five contiguous frequency zones and mirrors
// the five zone levels around the centre of the supplied span.
void render_symmetric_frequency_zones(LogicalRgbwPixels pixels,
                                      const SpectrumFrame& spectrum);

// Renders contiguous Bass, Mid, and High zones in red, green, and blue.
void render_bass_mid_high_zones(LogicalRgbwPixels pixels,
                                const SpectrumFrame& spectrum);

// Renders left and right VU levels from the centre outward. On an odd span the
// centre pixel belongs to Left and Right begins immediately to its right.
void render_stereo_center_out(LogicalRgbwPixels pixels,
                              uint16_t left_level,
                              uint16_t right_level,
                              RgbwColor left_color,
                              RgbwColor right_color);

// Renders one VU level from the beginning of the caller-provided span.
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
