#include "led/diagnostic_rendering.hpp"

#include <algorithm>
#include <array>

#include "audio/spectrum_resampler.hpp"

namespace {

constexpr std::size_t kSpectrumSegments = 16;
constexpr std::size_t kFrequencyZones = 5;

bool valid(LogicalRgbwPixels pixels) {
    return pixels.data != nullptr && pixels.size != 0;
}

uint8_t scale_channel(uint8_t channel, uint16_t level) {
    const uint32_t scaled = static_cast<uint32_t>(channel) * level + 32767u;
    return static_cast<uint8_t>(std::min<uint32_t>(
        255u, static_cast<uint32_t>(scaled / 65535u)));
}

RgbwColor scale_color(RgbwColor color, uint16_t level) {
    return {
        scale_channel(color.red, level),
        scale_channel(color.green, level),
        scale_channel(color.blue, level),
        scale_channel(color.white, level),
    };
}

std::size_t lit_pixels(std::size_t pixel_count, uint16_t level) {
    const uint64_t scaled = static_cast<uint64_t>(pixel_count) * level;
    return static_cast<std::size_t>(std::min<uint64_t>(pixel_count, scaled / 65535u));
}

uint8_t interpolate_channel(uint8_t first, uint8_t second, uint16_t fraction) {
    const uint32_t inverse = 65535u - fraction;
    return static_cast<uint8_t>(
        (static_cast<uint32_t>(first) * inverse +
         static_cast<uint32_t>(second) * fraction + 32767u) /
        65535u);
}

RgbwColor spectrum_color(std::size_t index, std::size_t count) {
    constexpr std::array<RgbwColor, 4> kStops{{
        {0, 0, 160, 96},
        {0, 200, 255, 0},
        {0, 255, 32, 0},
        {255, 32, 0, 0},
    }};

    if (count <= 1u) {
        return kStops.front();
    }

    const std::size_t scaled = index * (kStops.size() - 1u) * 65535u /
                               (count - 1u);
    const std::size_t first = scaled / 65535u;
    const uint16_t fraction = static_cast<uint16_t>(scaled % 65535u);
    const RgbwColor& low = kStops[first];
    const RgbwColor& high = kStops[std::min(first + 1u, kStops.size() - 1u)];
    return {
        interpolate_channel(low.red, high.red, fraction),
        interpolate_channel(low.green, high.green, fraction),
        interpolate_channel(low.blue, high.blue, fraction),
        interpolate_channel(low.white, high.white, fraction),
    };
}

}  // namespace

void clear_logical_pixels(LogicalRgbwPixels pixels) {
    if (!valid(pixels)) {
        return;
    }

    for (std::size_t index = 0; index < pixels.size; ++index) {
        pixels.data[index] = {0, 0, 0, 0};
    }
}

void render_spectrum_32_to_16(LogicalRgbwPixels pixels,
                              const SpectrumFrame& spectrum) {
    if (!valid(pixels)) {
        return;
    }

    std::array<uint16_t, kSpectrumSegments> segments{};
    resample_spectrum(spectrum, segments.data(), segments.size());

    for (std::size_t pixel = 0; pixel < pixels.size; ++pixel) {
        const std::size_t segment = pixel * kSpectrumSegments / pixels.size;
        pixels.data[pixel] = scale_color(
            spectrum_color(segment, kSpectrumSegments), segments[segment]);
    }
}

void render_symmetric_frequency_zones(LogicalRgbwPixels pixels,
                                      const SpectrumFrame& spectrum) {
    if (!valid(pixels)) {
        return;
    }

    std::array<uint16_t, kFrequencyZones> zones{};
    resample_spectrum(spectrum, zones.data(), zones.size());

    for (std::size_t pixel = 0; pixel < pixels.size; ++pixel) {
        const std::size_t from_nearest_end = std::min(pixel, pixels.size - 1u - pixel);
        const std::size_t half_span = (pixels.size + 1u) / 2u;
        const std::size_t zone = std::min(
            kFrequencyZones - 1u,
            from_nearest_end * kFrequencyZones / half_span);
        pixels.data[pixel] = scale_color(
            spectrum_color(zone, kFrequencyZones), zones[zone]);
    }
}

void render_bass_mid_high_zones(LogicalRgbwPixels pixels,
                                const SpectrumFrame& spectrum) {
    if (!valid(pixels)) {
        return;
    }

    const std::array<uint16_t, 3> levels{{spectrum.bass, spectrum.mid, spectrum.high}};
    const std::array<RgbwColor, 3> colors{{
        {255, 0, 0, 0},
        {0, 255, 0, 0},
        {0, 0, 255, 0},
    }};

    for (std::size_t pixel = 0; pixel < pixels.size; ++pixel) {
        const std::size_t zone = std::min<std::size_t>(2u, pixel * 3u / pixels.size);
        pixels.data[pixel] = scale_color(colors[zone], levels[zone]);
    }
}

void render_stereo_center_out(LogicalRgbwPixels pixels,
                              uint16_t left_level,
                              uint16_t right_level,
                              RgbwColor left_color,
                              RgbwColor right_color) {
    if (!valid(pixels)) {
        return;
    }

    clear_logical_pixels(pixels);

    const std::size_t left_capacity = (pixels.size + 1u) / 2u;
    const std::size_t right_capacity = pixels.size / 2u;
    const std::size_t left_lit = lit_pixels(left_capacity, left_level);
    const std::size_t right_lit = lit_pixels(right_capacity, right_level);
    const std::size_t centre_left = (pixels.size - 1u) / 2u;
    const std::size_t centre_right = (pixels.size + 1u) / 2u;

    for (std::size_t step = 0; step < left_lit; ++step) {
        pixels.data[centre_left - step] = left_color;
    }

    for (std::size_t step = 0; step < right_lit; ++step) {
        pixels.data[centre_right + step] = right_color;
    }
}

void render_full_vu(LogicalRgbwPixels pixels,
                    uint16_t level,
                    RgbwColor color) {
    if (!valid(pixels)) {
        return;
    }

    clear_logical_pixels(pixels);
    const std::size_t lit = lit_pixels(pixels.size, level);

    for (std::size_t pixel = 0; pixel < lit; ++pixel) {
        pixels.data[pixel] = color;
    }
}

bool diagnostic_frame_should_start(bool renderer_enabled,
                                   bool frame_in_progress,
                                   uint64_t now_us,
                                   uint64_t last_update_us,
                                   uint64_t interval_us) {
    return renderer_enabled && !frame_in_progress &&
           now_us - last_update_us >= interval_us;
}
