#pragma once

#include "audio/audio_processing.hpp"
#include "effects/effect_engine.hpp"

#include <array>
#include <cstdint>

namespace effects {

constexpr uint8_t kIdleActivityLeft = 1u << 0u;
constexpr uint8_t kIdleActivityRight = 1u << 1u;
constexpr uint8_t kIdleActivityAux = 1u << 2u;
constexpr uint8_t kIdleAllInputs =
    kIdleActivityLeft | kIdleActivityRight | kIdleActivityAux;
constexpr uint8_t kIdleAllStrips = (1u << kEffectStripCount) - 1u;
constexpr uint16_t kIdleMaximumTimingMs = 60000u;

enum class IdleLightingStatus : uint8_t {
    ok,
    invalid_parameter,
};

enum class IdleLightingState : uint8_t {
    effects,
    idle,
    fading_to_effects,
    fading_to_idle,
};

// idle_brightness_q8 is a Q8 multiplier: 0 is off and 256 is the configured
// RGBW idle colour at full logical channel value.
struct IdleLightingConfig {
    bool enabled = false;
    bool startup_idle_enabled = true;
    uint16_t silence_timeout_ms = 10000u;
    uint16_t audio_confirmation_ms = 150u;
    RgbwColor idle_color_rgbw{0u, 0u, 0u, 255u};
    uint16_t idle_brightness_q8 = kEffectUnityGain;
    uint16_t fade_to_effect_ms = 750u;
    uint16_t fade_to_idle_ms = 1500u;
    uint8_t activity_input_mask = kIdleAllInputs;
    uint8_t strip_enable_mask = kIdleAllStrips;
    uint16_t left_activity_floor = 32u;
    uint16_t right_activity_floor = 32u;
    uint16_t aux_activity_floor = 32u;
    uint16_t activity_hysteresis = 4u;
};

struct IdleLightingRuntime {
    uint16_t effect_mix = 65535u;
    IdleLightingState state = IdleLightingState::effects;
    uint32_t inactive_ms = 0u;
    bool left_active = false;
    bool right_active = false;
    bool aux_active = false;
    bool initialized = false;
    bool active_timer_valid = false;
    bool inactive_timer_valid = false;
    uint64_t last_timestamp_us = 0u;
    uint64_t active_since_us = 0u;
    uint64_t inactive_since_us = 0u;
};

class IdleLightingController {
public:
    bool read_config(IdleLightingConfig& output) const;
    IdleLightingStatus validate_config(const IdleLightingConfig& config) const;
    IdleLightingStatus stage_config(const IdleLightingConfig& config);
    bool has_pending_config() const;
    bool apply_pending_config();
    const IdleLightingRuntime& runtime() const;

    void update(const AudioLevelFrame& audio, uint64_t timestamp_us);
    void blend(const std::array<EffectRenderSpan, kEffectStripCount>& spans) const;

private:
    static bool update_gate(uint16_t raw,
                            uint16_t floor,
                            uint16_t hysteresis,
                            bool& active);
    static RgbwColor interpolate(RgbwColor idle, RgbwColor effect, uint16_t mix);
    static RgbwColor scale(RgbwColor color, uint16_t level);
    static uint16_t ramp(uint16_t current,
                         uint16_t target,
                         uint16_t duration_ms,
                         uint32_t elapsed_ms);
    static bool requires_runtime_reset(const IdleLightingConfig& active,
                                       const IdleLightingConfig& proposed);

    IdleLightingConfig active_{};
    IdleLightingConfig pending_{};
    IdleLightingRuntime runtime_{};
    bool pending_valid_ = false;
};

const char* effect_identifier(EffectType type);
const char* effect_display_name(EffectType type);
bool effect_supports_source(EffectType type, EffectSource source);

enum class EffectCategory : uint8_t { utility, static_light, vu, spectrum, frequency, ambient };
enum EffectParameterMask : uint32_t {
    effect_parameter_enabled = 1u << 0u,
    effect_parameter_source = 1u << 1u,
    effect_parameter_direction = 1u << 2u,
    effect_parameter_colours = 1u << 3u,
    effect_parameter_palette = 1u << 4u,
    effect_parameter_gain = 1u << 5u,
    effect_parameter_response = 1u << 6u,
    effect_parameter_animation = 1u << 7u,
    effect_parameter_geometry = 1u << 8u,
    effect_parameter_strobe = 1u << 9u,
    effect_parameter_gyver_adaptive = 1u << 10u,
};

struct EffectMetadata {
    const char* identifier;
    const char* display_name;
    EffectCategory category;
    uint32_t source_mask;
    uint32_t parameter_mask;
};

const EffectMetadata* effect_metadata(EffectType type);

}  // namespace effects
