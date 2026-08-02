#include "effects/effect_engine.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace effects {

namespace {

constexpr uint16_t kAudioPeakMaximum = 2047u;
constexpr uint32_t kDefaultRenderIntervalMs = 33u;
constexpr uint32_t kMaximumElapsedMs = 1000u;

bool supported_segment_count(uint8_t count) {
    return count == 5u || count == 8u || count == 16u || count == 32u;
}

bool valid_span(EffectRenderSpan span) {
    return span.pixels != nullptr && span.pixel_count != 0u;
}

uint16_t saturating_gain(uint16_t level, uint16_t gain) {
    const uint32_t scaled = static_cast<uint32_t>(level) * gain +
                            kEffectUnityGain / 2u;
    return static_cast<uint16_t>(std::min<uint32_t>(
        65535u, scaled / kEffectUnityGain));
}

uint8_t scale_channel(uint8_t channel, uint16_t level) {
    const uint32_t scaled = static_cast<uint32_t>(channel) * level + 32767u;
    return static_cast<uint8_t>(scaled / 65535u);
}

RgbwColor scale_color(RgbwColor color, uint16_t level) {
    return {
        scale_channel(color.red, level),
        scale_channel(color.green, level),
        scale_channel(color.blue, level),
        scale_channel(color.white, level),
    };
}

RgbwColor white_boost_color(const StripEffectConfig& config) {
    const uint16_t drive = std::min<uint16_t>(
        config.white_drive_percent, 200u);

    if (drive <= 100u) {
        const uint16_t white = static_cast<uint16_t>(
            (static_cast<uint32_t>(drive) * 255u + 50u) / 100u);
        return {0, 0, 0, static_cast<uint8_t>(white)};
    }

    const uint16_t assist_percent = static_cast<uint16_t>(drive - 100u);
    const RgbwColor assist = config.rgb_assist_color;
    const auto scale_assist_channel = [assist_percent](uint8_t channel) {
        return static_cast<uint8_t>(
            (static_cast<uint32_t>(channel) * assist_percent + 50u) / 100u);
    };

    return {
        scale_assist_channel(assist.red),
        scale_assist_channel(assist.green),
        scale_assist_channel(assist.blue),
        255u,
    };
}

RgbwColor level_color(RgbwColor foreground,
                      RgbwColor background,
                      uint16_t level) {
    return level == 0u ? background : scale_color(foreground, level);
}

void clear_span(EffectRenderSpan span) {
    if (span.pixels == nullptr) {
        return;
    }

    for (std::size_t index = 0; index < span.pixel_count; ++index) {
        span.pixels[index] = {0, 0, 0, 0};
    }
}

void fill_span(EffectRenderSpan span, RgbwColor color) {
    if (span.pixels == nullptr) {
        return;
    }

    for (std::size_t index = 0; index < span.pixel_count; ++index) {
        span.pixels[index] = color;
    }
}

void write_pixel(EffectRenderSpan span,
                 bool reversed,
                 std::size_t logical_index,
                 RgbwColor color) {
    if (!valid_span(span) || logical_index >= span.pixel_count) {
        return;
    }

    const std::size_t output_index = reversed
                                         ? span.pixel_count - 1u - logical_index
                                         : logical_index;
    span.pixels[output_index] = color;
}

uint16_t normalize_audio_peak(uint16_t peak) {
    const uint32_t clamped = std::min<uint32_t>(peak, kAudioPeakMaximum);
    return static_cast<uint16_t>(
        (clamped * 65535u + kAudioPeakMaximum / 2u) / kAudioPeakMaximum);
}

uint16_t scalar_level(EffectSource source, const EffectInputSnapshot& snapshot) {
    if (snapshot.audio == nullptr && snapshot.spectrum == nullptr) {
        return 0u;
    }

    if (snapshot.audio != nullptr) {
        switch (source) {
        case EffectSource::left:
            return normalize_audio_peak(snapshot.audio->left_peak);
        case EffectSource::right:
            return normalize_audio_peak(snapshot.audio->right_peak);
        case EffectSource::aux:
            return normalize_audio_peak(snapshot.audio->aux_peak);
        case EffectSource::mono:
            return normalize_audio_peak(snapshot.audio->mono_metrics.peak);
        default:
            break;
        }
    }

    if (snapshot.spectrum != nullptr) {
        switch (source) {
        case EffectSource::bass:
            return snapshot.spectrum->raw_bass;
        case EffectSource::low:
            return snapshot.spectrum->raw_low;
        case EffectSource::mid:
            return snapshot.spectrum->raw_mid;
        case EffectSource::high:
            return snapshot.spectrum->raw_high;
        default:
            break;
        }
    }

    return 0u;
}

uint32_t elapsed_ms(StripEffectState& state, uint64_t timestamp_us) {
    if (!state.initialized) {
        state.initialized = true;
        state.last_render_us = timestamp_us;
        return kDefaultRenderIntervalMs;
    }

    if (timestamp_us <= state.last_render_us) {
        return 0u;
    }

    const uint64_t elapsed_us = timestamp_us - state.last_render_us;
    state.last_render_us = timestamp_us;
    return static_cast<uint32_t>(std::min<uint64_t>(
        kMaximumElapsedMs, elapsed_us / 1000u));
}

uint16_t smooth_level(uint16_t previous,
                      uint16_t target,
                      uint16_t attack_ms,
                      uint16_t release_ms,
                      uint32_t elapsed) {
    const uint16_t response_ms = target > previous ? attack_ms : release_ms;
    if (response_ms == 0u) {
        return target;
    }

    const uint32_t alpha_q8 = std::min<uint32_t>(
        256u, (elapsed * 256u) / (static_cast<uint32_t>(response_ms) + elapsed));
    const int32_t difference = static_cast<int32_t>(target) - previous;
    if (difference == 0 || elapsed == 0u) {
        return previous;
    }

    int32_t magnitude =
        (std::abs(difference) * static_cast<int32_t>(alpha_q8) + 128) / 256;
    if (magnitude == 0) {
        magnitude = 1;
    }

    const int32_t adjustment = difference >= 0 ? magnitude : -magnitude;
    const int32_t result = static_cast<int32_t>(previous) + adjustment;
    return static_cast<uint16_t>(std::clamp<int32_t>(result, 0, 65535));
}

uint16_t update_level(StripEffectRuntime& runtime,
                      std::size_t state_index,
                      uint16_t target,
                      uint32_t elapsed) {
    if (state_index >= runtime.state.smoothed_levels.size()) {
        return 0u;
    }

    const uint16_t gained = saturating_gain(target, runtime.config.visual_gain);
    uint16_t& previous = runtime.state.smoothed_levels[state_index];
    previous = smooth_level(previous,
                            gained,
                            runtime.config.attack_ms,
                            runtime.config.release_ms,
                            elapsed);
    return previous;
}

uint8_t interpolate_channel(uint8_t first, uint8_t second, uint16_t fraction) {
    const uint32_t inverse = 65535u - fraction;
    return static_cast<uint8_t>(
        (static_cast<uint32_t>(first) * inverse +
         static_cast<uint32_t>(second) * fraction + 32767u) /
        65535u);
}

RgbwColor interpolate_color(RgbwColor first, RgbwColor second, uint16_t fraction) {
    return {
        interpolate_channel(first.red, second.red, fraction),
        interpolate_channel(first.green, second.green, fraction),
        interpolate_channel(first.blue, second.blue, fraction),
        interpolate_channel(first.white, second.white, fraction),
    };
}

bool palette_is_empty(const std::array<RgbwColor, 4>& palette) {
    for (const RgbwColor& color : palette) {
        if (color.red != 0u || color.green != 0u || color.blue != 0u ||
            color.white != 0u) {
            return false;
        }
    }

    return true;
}

constexpr uint16_t kRainbowHuePeriod = 1536u;
constexpr std::array<RgbwColor, 4> kDefaultVuGradient{{
    {0u, 255u, 0u, 0u},
    {255u, 255u, 0u, 0u},
    {255u, 112u, 0u, 0u},
    {255u, 0u, 0u, 0u},
}};

RgbwColor rainbow_color(uint16_t hue) {
    const uint16_t wrapped = static_cast<uint16_t>(hue % kRainbowHuePeriod);
    const uint8_t segment = static_cast<uint8_t>(wrapped / 256u);
    const uint8_t fraction = static_cast<uint8_t>(wrapped % 256u);

    switch (segment) {
    case 0u:
        return {255u, fraction, 0u, 0u};
    case 1u:
        return {static_cast<uint8_t>(255u - fraction), 255u, 0u, 0u};
    case 2u:
        return {0u, 255u, fraction, 0u};
    case 3u:
        return {0u, static_cast<uint8_t>(255u - fraction), 255u, 0u};
    case 4u:
        return {fraction, 0u, 255u, 0u};
    default:
        return {255u, 0u, static_cast<uint8_t>(255u - fraction), 0u};
    }
}

void advance_vu_rainbow_phase(StripEffectState& state,
                              uint16_t speed_q8,
                              uint32_t elapsed) {
    const uint32_t increment =
        (static_cast<uint32_t>(speed_q8) * elapsed + 128u) / 256u;
    state.animation_phase = static_cast<uint16_t>(
        (state.animation_phase + increment) % kRainbowHuePeriod);
}

RgbwColor vu_gradient_color(const StripEffectConfig& config,
                            std::size_t distance_from_origin,
                            std::size_t capacity) {
    const std::array<RgbwColor, 4>& palette = palette_is_empty(config.palette)
                                                   ? kDefaultVuGradient
                                                   : config.palette;
    if (capacity <= 1u) {
        return palette.front();
    }

    const std::size_t clamped_distance = std::min(
        distance_from_origin, capacity - 1u);
    const std::size_t scaled = clamped_distance * (palette.size() - 1u) *
                               65535u / (capacity - 1u);
    const std::size_t first = scaled / 65535u;
    const uint16_t fraction = static_cast<uint16_t>(scaled % 65535u);
    return interpolate_color(palette[first],
                             palette[std::min(first + 1u, palette.size() - 1u)],
                             fraction);
}

RgbwColor vu_color(const StripEffectRuntime& runtime,
                   RgbwColor solid_color,
                   std::size_t distance_from_origin,
                   std::size_t capacity) {
    switch (runtime.config.vu_color_mode) {
    case VuColorMode::solid:
        return solid_color;

    case VuColorMode::level_position_gradient:
        return vu_gradient_color(runtime.config, distance_from_origin, capacity);

    case VuColorMode::animated_rainbow: {
        const uint32_t spacing = static_cast<uint32_t>(distance_from_origin) *
                                 runtime.config.color_spacing_q8;
        const uint16_t hue_offset = static_cast<uint16_t>(
            ((spacing + 128u) / 256u) % kRainbowHuePeriod);
        const uint16_t hue = static_cast<uint16_t>(
            (static_cast<uint32_t>(runtime.state.animation_phase) + hue_offset) %
            kRainbowHuePeriod);
        return rainbow_color(hue);
    }
    }

    return solid_color;
}

RgbwColor spectrum_color(const StripEffectConfig& config,
                         std::size_t index,
                         std::size_t count) {
    std::array<RgbwColor, 4> palette = config.palette;
    if (palette_is_empty(palette)) {
        palette[0] = config.primary_color;
        palette[1] = config.secondary_color;
        palette[2] = config.secondary_color;
        palette[3] = config.secondary_color;
    }

    if (count <= 1u) {
        return palette.front();
    }

    const std::size_t scaled =
        index * (palette.size() - 1u) * 65535u / (count - 1u);
    const std::size_t first = scaled / 65535u;
    const uint16_t fraction = static_cast<uint16_t>(scaled % 65535u);
    return interpolate_color(palette[first],
                             palette[std::min(first + 1u, palette.size() - 1u)],
                             fraction);
}

void resample_raw_spectrum(const SpectrumFrame* spectrum,
                           uint16_t* destination,
                           std::size_t segment_count) {
    if (destination == nullptr || segment_count == 0u ||
        segment_count > kSpectrumBandCount) {
        return;
    }

    for (std::size_t destination_index = 0;
         destination_index < segment_count;
         ++destination_index) {
        uint32_t sum = 0u;
        uint32_t count = 0u;
        const std::size_t first = destination_index * kSpectrumBandCount;
        const std::size_t last = (destination_index + 1u) * kSpectrumBandCount;

        for (std::size_t scaled_index = first;
             scaled_index < last;
             ++scaled_index) {
            const std::size_t source_index = scaled_index / segment_count;
            sum += spectrum == nullptr ? 0u : spectrum->raw_bands[source_index];
            ++count;
        }

        destination[destination_index] = static_cast<uint16_t>(sum / count);
    }
}

std::size_t lit_count(std::size_t capacity, uint16_t level) {
    return static_cast<std::size_t>(
        static_cast<uint64_t>(capacity) * level / 65535u);
}

void render_scalar_vu(StripEffectRuntime& runtime,
                      const EffectInputSnapshot& snapshot,
                      EffectRenderSpan destination,
                      uint32_t elapsed) {
    fill_span(destination, runtime.config.background_color);
    const uint16_t level = update_level(runtime,
                                        0u,
                                        scalar_level(runtime.config.source, snapshot),
                                        elapsed);
    const std::size_t lit = lit_count(destination.pixel_count, level);
    if (runtime.config.vu_color_mode == VuColorMode::animated_rainbow) {
        advance_vu_rainbow_phase(runtime.state,
                                  runtime.config.animation_speed_q8,
                                  elapsed);
    }

    for (std::size_t index = 0; index < lit; ++index) {
        write_pixel(destination,
                    runtime.config.reversed,
                    index,
                    vu_color(runtime,
                             runtime.config.primary_color,
                             index,
                             destination.pixel_count));
    }
}

void render_stereo_vu(StripEffectRuntime& runtime,
                      const EffectInputSnapshot& snapshot,
                      EffectRenderSpan destination,
                      uint32_t elapsed) {
    fill_span(destination, runtime.config.background_color);
    const uint16_t left = update_level(runtime,
                                       0u,
                                       scalar_level(EffectSource::left, snapshot),
                                       elapsed);
    const uint16_t right = update_level(runtime,
                                        1u,
                                        scalar_level(EffectSource::right, snapshot),
                                        elapsed);
    const std::size_t left_capacity = (destination.pixel_count + 1u) / 2u;
    const std::size_t right_capacity = destination.pixel_count / 2u;
    const std::size_t left_lit = lit_count(left_capacity, left);
    const std::size_t right_lit = lit_count(right_capacity, right);
    const std::size_t centre_left = (destination.pixel_count - 1u) / 2u;
    const std::size_t centre_right = (destination.pixel_count + 1u) / 2u;

    if (runtime.config.vu_color_mode == VuColorMode::animated_rainbow) {
        advance_vu_rainbow_phase(runtime.state,
                                  runtime.config.animation_speed_q8,
                                  elapsed);
    }

    for (std::size_t step = 0; step < left_lit; ++step) {
        write_pixel(destination,
                    runtime.config.reversed,
                    centre_left - step,
                    vu_color(runtime,
                             runtime.config.primary_color,
                             step,
                             left_capacity));
    }

    for (std::size_t step = 0; step < right_lit; ++step) {
        write_pixel(destination,
                    runtime.config.reversed,
                    centre_right + step,
                    vu_color(runtime,
                             runtime.config.secondary_color,
                             step,
                             right_capacity));
    }
}

void render_spectrum_bars(StripEffectRuntime& runtime,
                          const EffectInputSnapshot& snapshot,
                          EffectRenderSpan destination,
                          uint32_t elapsed) {
    fill_span(destination, runtime.config.background_color);
    std::array<uint16_t, kSpectrumBandCount> segments{};
    const std::size_t segment_count = runtime.config.segment_count;
    if (!supported_segment_count(runtime.config.segment_count)) {
        return;
    }

    resample_raw_spectrum(snapshot.spectrum, segments.data(), segment_count);

    std::array<uint16_t, kSpectrumBandCount> smoothed_segments{};
    for (std::size_t source_segment = 0u;
         source_segment < segment_count;
         ++source_segment) {
        smoothed_segments[source_segment] = update_level(runtime,
                                                          source_segment,
                                                          segments[source_segment],
                                                          elapsed);
    }

    for (std::size_t pixel = 0; pixel < destination.pixel_count; ++pixel) {
        const std::size_t physical_segment = pixel * segment_count /
                                             destination.pixel_count;
        const std::size_t source_segment = runtime.config.reversed
                                               ? segment_count - 1u - physical_segment
                                               : physical_segment;
        write_pixel(destination,
                    false,
                    pixel,
                    level_color(spectrum_color(runtime.config,
                                               source_segment,
                                               segment_count),
                                runtime.config.background_color,
                                smoothed_segments[source_segment]));
    }
}

void render_mirrored_zones(StripEffectRuntime& runtime,
                           const EffectInputSnapshot& snapshot,
                           EffectRenderSpan destination,
                           uint32_t elapsed) {
    fill_span(destination, runtime.config.background_color);
    std::array<uint16_t, kSpectrumBandCount> zones{};
    const std::size_t zone_count = runtime.config.zone_count;
    if (zone_count == 0u || zone_count > kEffectMaximumMirroredZones) {
        return;
    }

    resample_raw_spectrum(snapshot.spectrum, zones.data(), zone_count);
    std::array<uint16_t, kSpectrumBandCount> smoothed_zones{};
    for (std::size_t source_zone = 0u;
         source_zone < zone_count;
         ++source_zone) {
        smoothed_zones[source_zone] = update_level(runtime,
                                                    source_zone,
                                                    zones[source_zone],
                                                    elapsed);
    }

    const std::size_t half_span = (destination.pixel_count + 1u) / 2u;

    for (std::size_t pixel = 0; pixel < destination.pixel_count; ++pixel) {
        const std::size_t from_nearest_end = std::min(
            pixel, destination.pixel_count - 1u - pixel);
        const std::size_t zone_from_end = std::min(
            zone_count - 1u,
            from_nearest_end * zone_count / half_span);
        const std::size_t source_zone = runtime.config.reversed
                                            ? zone_count - 1u - zone_from_end
                                            : zone_from_end;
        write_pixel(destination,
                    false,
                    pixel,
                    level_color(spectrum_color(runtime.config,
                                               source_zone,
                                               zone_count),
                                runtime.config.background_color,
                                smoothed_zones[source_zone]));
    }
}

void render_macro_bands(StripEffectRuntime& runtime,
                        const EffectInputSnapshot& snapshot,
                        EffectRenderSpan destination,
                        uint32_t elapsed) {
    fill_span(destination, runtime.config.background_color);
    const SpectrumFrame* spectrum = snapshot.spectrum;
    const std::array<uint16_t, 4> levels{{
        static_cast<uint16_t>(spectrum == nullptr ? 0u : spectrum->raw_bass),
        static_cast<uint16_t>(spectrum == nullptr ? 0u : spectrum->raw_low),
        static_cast<uint16_t>(spectrum == nullptr ? 0u : spectrum->raw_mid),
        static_cast<uint16_t>(spectrum == nullptr ? 0u : spectrum->raw_high),
    }};
    const std::size_t region_count = runtime.config.macro_region_count;
    if (region_count != 3u && region_count != 4u) {
        return;
    }

    std::array<uint16_t, 4> smoothed_regions{};
    for (std::size_t region = 0u; region < region_count; ++region) {
        const std::size_t source_region = region_count == 3u && region != 0u
                                              ? region + 1u
                                              : region;
        smoothed_regions[region] = update_level(runtime,
                                                region,
                                                levels[source_region],
                                                elapsed);
    }

    for (std::size_t pixel = 0; pixel < destination.pixel_count; ++pixel) {
        const std::size_t mapped_pixel = runtime.config.reversed
                                             ? destination.pixel_count - 1u - pixel
                                             : pixel;
        const std::size_t region = std::min(
            region_count - 1u,
            mapped_pixel * region_count / destination.pixel_count);
        write_pixel(destination,
                    false,
                    pixel,
                    level_color(runtime.config.palette[region],
                                runtime.config.background_color,
                                smoothed_regions[region]));
    }
}

uint16_t selected_frequency_level(FrequencySelection selection,
                                  const EffectInputSnapshot& snapshot) {
    if (snapshot.spectrum == nullptr) {
        return 0u;
    }

    switch (selection) {
    case FrequencySelection::three_frequencies:
        return std::max({snapshot.spectrum->raw_low,
                         snapshot.spectrum->raw_mid,
                         snapshot.spectrum->raw_high});

    case FrequencySelection::low:
        return snapshot.spectrum->raw_low;

    case FrequencySelection::mid:
        return snapshot.spectrum->raw_mid;

    case FrequencySelection::high:
        return snapshot.spectrum->raw_high;
    }

    return 0u;
}

void advance_animation_phase(StripEffectRuntime& runtime, uint32_t elapsed) {
    const uint32_t phase_increment =
        (static_cast<uint32_t>(runtime.config.animation_speed_q8) * elapsed +
         128u) /
        256u;
    runtime.state.animation_phase = static_cast<uint16_t>(
        (runtime.state.animation_phase + phase_increment) % kRainbowHuePeriod);
}

uint16_t pixel_hue_offset(const StripEffectConfig& config,
                          std::size_t pixel) {
    return static_cast<uint16_t>(
        ((static_cast<uint32_t>(pixel) * config.color_spacing_q8 + 128u) / 256u) %
        kRainbowHuePeriod);
}

void render_one_band_frequency(StripEffectRuntime& runtime,
                               const EffectInputSnapshot& snapshot,
                               EffectRenderSpan destination,
                               uint32_t elapsed) {
    const uint16_t level = update_level(runtime,
                                        0u,
                                        selected_frequency_level(
                                            runtime.config.frequency_selection,
                                            snapshot),
                                        elapsed);
    fill_span(destination,
              level_color(runtime.config.primary_color,
                          runtime.config.background_color,
                          level));
}

void render_stroboscope(StripEffectRuntime& runtime,
                        EffectRenderSpan destination,
                        uint32_t elapsed) {
    // ColorMusic's stroboscope is a timed lighting mode. It deliberately has
    // no audio source; brightness comes from the time gate alone.
    const uint16_t target = saturating_gain(65535u,
                                            runtime.config.visual_gain);
    const uint32_t cycle_ms = runtime.config.strobe_frequency_hz == 0u
                                  ? 1000u
                                  : 1000u / runtime.config.strobe_frequency_hz;
    const uint32_t phase_increment = cycle_ms == 0u
                                         ? 0u
                                         : (elapsed * 65535u + cycle_ms / 2u) /
                                               cycle_ms;
    runtime.state.strobe_phase = static_cast<uint16_t>(
        runtime.state.strobe_phase + phase_increment);

    const uint32_t fade_fraction = std::min<uint32_t>(
        65535u,
        (static_cast<uint32_t>(runtime.config.strobe_fade_ms) * 65535u +
         cycle_ms / 2u) /
            std::max<uint32_t>(1u, cycle_ms));
    uint16_t gate = 0u;
    if (runtime.state.strobe_phase < fade_fraction) {
        gate = fade_fraction == 0u
                   ? 65535u
                   : static_cast<uint16_t>(
                         65535u - static_cast<uint32_t>(runtime.state.strobe_phase) *
                                      65535u / fade_fraction);
    }

    runtime.state.strobe_level = static_cast<uint16_t>(
        (static_cast<uint32_t>(target) * gate + 32767u) / 65535u);
    fill_span(destination,
              level_color(runtime.config.primary_color,
                          runtime.config.background_color,
                          runtime.state.strobe_level));
}

void render_ambient_color_cycle(StripEffectRuntime& runtime,
                                EffectRenderSpan destination,
                                uint32_t elapsed) {
    advance_animation_phase(runtime, elapsed);
    fill_span(destination,
              scale_color(rainbow_color(runtime.state.animation_phase),
                          saturating_gain(65535u,
                                          runtime.config.visual_gain)));
}

void render_running_rainbow(StripEffectRuntime& runtime,
                            EffectRenderSpan destination,
                            uint32_t elapsed) {
    advance_animation_phase(runtime, elapsed);
    for (std::size_t pixel = 0u; pixel < destination.pixel_count; ++pixel) {
        const std::size_t gradient_pixel = runtime.config.reversed
                                               ? destination.pixel_count - 1u - pixel
                                               : pixel;
        const uint16_t hue = static_cast<uint16_t>(
            (static_cast<uint32_t>(runtime.state.animation_phase) +
             pixel_hue_offset(runtime.config, gradient_pixel)) %
            kRainbowHuePeriod);
        write_pixel(destination,
                    false,
                    pixel,
                    scale_color(rainbow_color(hue),
                                saturating_gain(65535u,
                                                runtime.config.visual_gain)));
    }
}

void render_running_frequency(StripEffectRuntime& runtime,
                              const EffectInputSnapshot& snapshot,
                              EffectRenderSpan destination,
                              uint32_t elapsed) {
    fill_span(destination, runtime.config.background_color);
    if (destination.pixel_count == 0u) {
        return;
    }

    const uint16_t target = saturating_gain(
        selected_frequency_level(runtime.config.frequency_selection, snapshot),
        runtime.config.visual_gain);
    const uint32_t decay_period = std::max<uint32_t>(1u, runtime.config.fade_decay_ms);
    const uint32_t retained_q16 = elapsed >= decay_period
                                      ? 0u
                                      : (decay_period - elapsed) * 65535u /
                                            decay_period;
    runtime.state.running_level = static_cast<uint16_t>(std::max<uint32_t>(
        target,
        (static_cast<uint32_t>(runtime.state.running_level) * retained_q16 +
         32767u) /
            65535u));

    const uint32_t position_increment =
        (static_cast<uint32_t>(runtime.config.animation_speed_q8) * elapsed +
         128u) /
        256u;
    const uint32_t span_q8 = static_cast<uint32_t>(destination.pixel_count) * 256u;
    runtime.state.running_position = static_cast<uint32_t>(
        (static_cast<uint32_t>(runtime.state.running_position) + position_increment) %
        span_q8);
    const std::size_t head = runtime.state.running_position / 256u;

    for (std::size_t pixel = 0u; pixel < destination.pixel_count; ++pixel) {
        const std::size_t distance = runtime.config.reversed
                                         ? (head + destination.pixel_count - pixel) %
                                               destination.pixel_count
                                         : (pixel + destination.pixel_count - head) %
                                               destination.pixel_count;
        const uint16_t trail = static_cast<uint16_t>(
            (static_cast<uint32_t>(runtime.state.running_level) *
             (destination.pixel_count - distance) + destination.pixel_count / 2u) /
            destination.pixel_count);
        write_pixel(destination,
                    false,
                    pixel,
                    level_color(runtime.config.primary_color,
                                runtime.config.background_color,
                                trail));
    }
}

}  // namespace

void render_effect(StripEffectRuntime& runtime,
                   const EffectInputSnapshot& snapshot,
                   EffectRenderSpan destination) {
    if (!valid_span(destination)) {
        return;
    }

    if (runtime.config.type == EffectType::off) {
        clear_span(destination);
        return;
    }

    if (runtime.config.type == EffectType::static_rgbw) {
        const RgbwColor static_color =
            runtime.config.static_color_mode == StaticColorMode::white_boost
                ? white_boost_color(runtime.config)
                : runtime.config.primary_color;
        fill_span(destination,
                  scale_color(static_color,
                              saturating_gain(65535u,
                                              runtime.config.visual_gain)));
        return;
    }

    const uint32_t elapsed = elapsed_ms(runtime.state, snapshot.timestamp_us);

    switch (runtime.config.type) {
    case EffectType::off:
    case EffectType::static_rgbw:
        return;

    case EffectType::scalar_vu:
        render_scalar_vu(runtime, snapshot, destination, elapsed);
        return;

    case EffectType::stereo_center_out_vu:
        render_stereo_vu(runtime, snapshot, destination, elapsed);
        return;

    case EffectType::spectrum_bars:
        render_spectrum_bars(runtime, snapshot, destination, elapsed);
        return;

    case EffectType::mirrored_spectrum_zones:
        render_mirrored_zones(runtime, snapshot, destination, elapsed);
        return;

    case EffectType::macro_bands:
        render_macro_bands(runtime, snapshot, destination, elapsed);
        return;

    case EffectType::one_band_frequency:
        render_one_band_frequency(runtime, snapshot, destination, elapsed);
        return;

    case EffectType::stroboscope:
        render_stroboscope(runtime, destination, elapsed);
        return;

    case EffectType::ambient_color_cycle:
        render_ambient_color_cycle(runtime, destination, elapsed);
        return;

    case EffectType::running_rainbow:
        render_running_rainbow(runtime, destination, elapsed);
        return;

    case EffectType::running_frequency:
        render_running_frequency(runtime, snapshot, destination, elapsed);
        return;
    }
}

}  // namespace effects
