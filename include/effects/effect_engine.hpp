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

enum class EffectType : uint8_t {
    off,
    static_rgbw,
    scalar_vu,
    stereo_center_out_vu,
    spectrum_bars,
    mirrored_spectrum_zones,
    macro_bands,
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
    uint64_t last_render_us = 0;
    uint32_t last_input_sequence = 0;
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
