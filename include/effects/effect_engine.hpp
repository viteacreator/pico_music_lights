#pragma once

#include "audio/audio_processing.hpp"
#include "audio/spectrum_analyzer.hpp"
#include "led/rgbw_color.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace effects {

constexpr std::size_t kEffectStripCount = 6;
constexpr uint16_t kEffectUnityGain = 256u;
constexpr uint16_t kEffectMaximumGain = 1024u;
constexpr uint16_t kEffectMaximumResponseMs = 5000u;
constexpr uint8_t kEffectMaximumMirroredZones = 16u;
constexpr std::size_t kEffectMaximumPixelsPerStrip = 300u;
constexpr std::size_t kEffectMaximumHalfPixelsPerStrip =
    (kEffectMaximumPixelsPerStrip + 1u) / 2u;
constexpr std::size_t kEffectAdaptiveMacroBandCount = 3u;

enum class EffectType : uint8_t {
    // Generic, extended effects.
    off,
    static_rgbw,
    scalar_vu,
    stereo_center_out_vu,
    spectrum_bars,
    mirrored_spectrum_zones,
    macro_bands,
    one_band_frequency,
    stroboscope,
    ambient_color_cycle,
    running_rainbow,
    frequency_comet,

    // AlexGyver ColorMusic-compatible effects. These remain distinct from the
    // generic effects so future configuration metadata can present both sets.
    gyver_vu_gradient,
    gyver_vu_rainbow,
    gyver_frequency_5_zones,
    gyver_frequency_3_zones,
    gyver_frequency_full_strip,
    gyver_stroboscope,
    gyver_ambient_static,
    gyver_ambient_color_cycle,
    gyver_ambient_running_rainbow,
    gyver_running_frequencies,
    gyver_spectrum_analyzer,
};

enum class VuColorMode : uint8_t {
    solid,
    level_position_gradient,
    animated_rainbow,
};

enum class FrequencySelection : uint8_t {
    three_frequencies,
    low,
    mid,
    high,
};

enum class EffectSource : uint8_t {
    none,
    left,
    right,
    aux,
    mono,
    bass,
    low,
    mid,
    high,
    stereo_left_right,
    spectrum_32,
    macro_bands,
};

enum class StaticColorMode : uint8_t {
    direct_rgbw,
    white_boost,
};

enum class GyverFullStripSelectionPolicy : uint8_t {
    gyver_priority,
    strongest_event,
};

enum class EffectStatus : uint8_t {
    ok,
    invalid_strip_index,
    invalid_effect_type,
    invalid_source,
    invalid_static_color_mode,
    incompatible_source,
    invalid_parameter,
};

// Every configuration is fixed-size and contains no dynamically allocated
// data. visual_gain uses kEffectUnityGain as unity. Response times are in
// milliseconds and are applied by the selected effect's private strip state.
struct StripEffectConfig {
    bool enabled = false;
    EffectType type = EffectType::off;
    EffectSource source = EffectSource::none;
    RgbwColor primary_color{0, 0, 0, 0};
    RgbwColor secondary_color{0, 0, 0, 0};
    RgbwColor background_color{0, 0, 0, 0};
    std::array<RgbwColor, 4> palette{};
    StaticColorMode static_color_mode = StaticColorMode::direct_rgbw;
    // White Boost accepts 0..200. Its RGB assist must have white == 0.
    uint16_t white_drive_percent = 100u;
    RgbwColor rgb_assist_color{255, 255, 255, 0};
    // Low, Mid, High in that exact order for Alex frequency effects.
    std::array<RgbwColor, kEffectAdaptiveMacroBandCount> gyver_frequency_colors{
        RgbwColor{0, 255, 0, 0},
        RgbwColor{255, 160, 0, 0},
        RgbwColor{255, 0, 0, 0},
    };
    VuColorMode vu_color_mode = VuColorMode::solid;
    FrequencySelection frequency_selection = FrequencySelection::three_frequencies;
    GyverFullStripSelectionPolicy gyver_full_strip_selection =
        GyverFullStripSelectionPolicy::gyver_priority;
    // Q8 phase increments per millisecond and Q8 pixel hue spacing.
    uint16_t animation_speed_q8 = 256u;
    uint16_t color_spacing_q8 = 256u;
    uint16_t fade_decay_ms = 180u;
    uint8_t strobe_frequency_hz = 8u;
    uint8_t strobe_duty_percent = 50u;
    uint16_t strobe_fade_ms = 40u;
    // Q8 scale applied to background_color; zero keeps the background off.
    uint16_t background_brightness_q8 = 0u;
    // Gyver-compatible adaptive gain and macro-event detector parameters.
    bool auto_gain_enabled = true;
    // Q8 multiplier; 461 is approximately 1.8x headroom.
    uint16_t auto_gain_headroom_q8 = 461u;
    uint16_t adaptive_fast_response_ms = 40u;
    uint16_t adaptive_average_response_ms = 700u;
    uint16_t adaptive_trigger_percent = 125u;
    uint16_t adaptive_event_decay_ms = 180u;
    uint16_t gyver_animation_interval_ms = 33u;
    bool reversed = false;
    uint16_t visual_gain = kEffectUnityGain;
    uint16_t attack_ms = 0;
    uint16_t release_ms = 0;
    uint8_t segment_count = 16;
    uint8_t zone_count = 5;
    uint8_t macro_region_count = 4;
};

// This state belongs to exactly one runtime strip. Effect implementations use
// the bounded value slots for their temporal response and never share mutable
// state with another strip.
struct StripEffectState {
    std::array<uint16_t, 32> smoothed_levels{};
    // Low, Mid and High adaptive detector state, owned by this strip only.
    std::array<uint16_t, kEffectAdaptiveMacroBandCount> adaptive_fast_levels{};
    std::array<uint16_t, kEffectAdaptiveMacroBandCount> adaptive_average_levels{};
    std::array<uint16_t, kEffectAdaptiveMacroBandCount> adaptive_event_levels{};
    // Left, Right and spectrum adaptive display references respectively.
    std::array<uint16_t, kEffectAdaptiveMacroBandCount> auto_gain_references{};
    // Only one logical half is retained for mirrored Alex running frequencies.
    // At 150 RGBW entries this costs 600 bytes per strip rather than 1,200.
    std::array<RgbwColor, kEffectMaximumHalfPixelsPerStrip>
        gyver_running_frequency_history{};
    uint16_t gyver_running_frequency_history_length = 0u;
    uint16_t gyver_animation_elapsed_ms = 0u;
    uint64_t last_render_us = 0;
    uint32_t last_input_sequence = 0;
    uint16_t animation_phase = 0;
    uint16_t strobe_phase = 0;
    uint16_t strobe_level = 0;
    // Q8 pixel position; 32 bits cover the full 300-pixel strip span.
    uint32_t running_position = 0;
    uint16_t running_level = 0;
    bool initialized = false;
};

struct StripEffectRuntime {
    StripEffectConfig config{};
    StripEffectState state{};
};

// The application supplies one coherent, read-only pair of frames for a
// complete effect-engine render. Effects must not retain these pointers after
// the render call returns.
struct EffectInputSnapshot {
    const AudioLevelFrame* audio = nullptr;
    const SpectrumFrame* spectrum = nullptr;
    uint64_t timestamp_us = 0;
};

// A caller-owned logical LED span. The engine and its effects may write only
// elements in [pixels, pixels + pixel_count).
struct EffectRenderSpan {
    RgbwColor* pixels = nullptr;
    std::size_t pixel_count = 0;
};

// Pure effect dispatch seam. Its implementation lives with the reusable
// rendering primitives and has no hardware dependencies.
void render_effect(StripEffectRuntime& runtime,
                   const EffectInputSnapshot& snapshot,
                   EffectRenderSpan destination);

class EffectEngine {
public:
    EffectEngine();

    bool read_config(std::size_t strip_index, StripEffectConfig& output) const;
    EffectStatus validate_config(const StripEffectConfig& config) const;
    EffectStatus stage_strip_config(std::size_t strip_index,
                                   const StripEffectConfig& config);
    EffectStatus stage_scene(
        const std::array<StripEffectConfig, kEffectStripCount>& scene);
    void restore_default_scene();
    bool has_pending_configuration() const;
    bool apply_pending();

    uint32_t configuration_generation() const;
    const StripEffectRuntime* runtime(std::size_t strip_index) const;

    void render(const EffectInputSnapshot& snapshot,
                const std::array<EffectRenderSpan, kEffectStripCount>& spans);

private:
    static std::array<StripEffectConfig, kEffectStripCount> default_scene();
    static void reset_state(StripEffectState& state);
    static void clear_span(EffectRenderSpan span);

    std::array<StripEffectRuntime, kEffectStripCount> active_{};
    std::array<StripEffectConfig, kEffectStripCount> pending_{};
    uint32_t configuration_generation_ = 0;
    bool pending_valid_ = false;
};

}  // namespace effects
