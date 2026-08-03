#include "effects/idle_lighting.hpp"

#include <algorithm>

namespace effects {

namespace {

constexpr uint16_t kRawPeakMaximum = 2047u;
constexpr uint32_t kMaximumElapsedMs = 1000u;

constexpr uint32_t source_bit(EffectSource source) {
    return 1u << static_cast<uint8_t>(source);
}

constexpr uint32_t kNoSource = source_bit(EffectSource::none);
constexpr uint32_t kLinearSources = source_bit(EffectSource::left) |
                                    source_bit(EffectSource::right) |
                                    source_bit(EffectSource::aux) |
                                    source_bit(EffectSource::mono);
constexpr uint32_t kStereoSource = source_bit(EffectSource::stereo_left_right);
constexpr uint32_t kSpectrumSource = source_bit(EffectSource::spectrum_32);
constexpr uint32_t kMacroSource = source_bit(EffectSource::macro_bands);

constexpr uint32_t kBasic = effect_parameter_enabled | effect_parameter_colours |
                            effect_parameter_gain;
constexpr uint32_t kVu = kBasic | effect_parameter_source |
                         effect_parameter_direction | effect_parameter_palette |
                         effect_parameter_response | effect_parameter_animation;
constexpr uint32_t kSpectrum = kBasic | effect_parameter_source |
                               effect_parameter_direction | effect_parameter_palette |
                               effect_parameter_response | effect_parameter_geometry;
constexpr uint32_t kFrequency = kBasic | effect_parameter_source |
                                effect_parameter_direction | effect_parameter_response |
                                effect_parameter_animation | effect_parameter_gyver_adaptive;

constexpr std::array<EffectMetadata, 23> kMetadata{{
    {"off", "Off", EffectCategory::utility, kNoSource, effect_parameter_enabled},
    {"static_rgbw", "Static RGBW", EffectCategory::static_light, kNoSource, kBasic},
    {"linear_vu", "Linear VU", EffectCategory::vu, kLinearSources, kVu},
    {"stereo_center_out_vu", "Stereo Centre-Out VU", EffectCategory::vu, kStereoSource, kVu},
    {"spectrum_bars", "Spectrum Bars", EffectCategory::spectrum, kSpectrumSource, kSpectrum},
    {"mirrored_spectrum_zones", "Mirrored Spectrum Zones", EffectCategory::spectrum, kSpectrumSource, kSpectrum},
    {"macro_bands", "Macro Bands", EffectCategory::spectrum, kMacroSource, kSpectrum},
    {"one_band_frequency", "One-Band Frequency", EffectCategory::frequency, kMacroSource, kFrequency},
    {"stroboscope", "Stroboscope", EffectCategory::ambient, kNoSource, kBasic | effect_parameter_strobe},
    {"ambient_color_cycle", "Ambient Colour Cycle", EffectCategory::ambient, kNoSource, kBasic | effect_parameter_animation},
    {"running_rainbow", "Running Rainbow", EffectCategory::ambient, kNoSource, kBasic | effect_parameter_direction | effect_parameter_animation},
    {"frequency_comet", "Frequency Comet", EffectCategory::frequency, kMacroSource, kFrequency},
    {"gyver_vu_gradient", "Gyver VU Gradient", EffectCategory::vu, kStereoSource, kVu | effect_parameter_gyver_adaptive},
    {"gyver_vu_rainbow", "Gyver VU Rainbow", EffectCategory::vu, kStereoSource, kVu | effect_parameter_gyver_adaptive},
    {"gyver_frequency_5_zones", "Gyver Frequency 5 Zones", EffectCategory::frequency, kMacroSource, kFrequency},
    {"gyver_frequency_3_zones", "Gyver Frequency 3 Zones", EffectCategory::frequency, kMacroSource, kFrequency},
    {"gyver_frequency_full_strip", "Gyver Frequency Full Strip", EffectCategory::frequency, kMacroSource, kFrequency},
    {"gyver_stroboscope", "Gyver Stroboscope", EffectCategory::ambient, kNoSource, kBasic | effect_parameter_strobe},
    {"gyver_ambient_static", "Gyver Ambient Static", EffectCategory::ambient, kNoSource, kBasic},
    {"gyver_ambient_color_cycle", "Gyver Ambient Colour Cycle", EffectCategory::ambient, kNoSource, kBasic | effect_parameter_animation},
    {"gyver_ambient_running_rainbow", "Gyver Ambient Running Rainbow", EffectCategory::ambient, kNoSource, kBasic | effect_parameter_animation},
    {"gyver_running_frequencies", "Gyver Running Frequencies", EffectCategory::frequency, kMacroSource, kFrequency},
    {"gyver_spectrum_analyzer", "Gyver Spectrum Analyzer", EffectCategory::spectrum, kSpectrumSource, kSpectrum | effect_parameter_gyver_adaptive},
}};

uint8_t scale_channel(uint8_t channel, uint16_t level) {
    return static_cast<uint8_t>(
        (static_cast<uint32_t>(channel) * level + 32767u) / 65535u);
}

uint8_t interpolate_channel(uint8_t idle, uint8_t effect, uint16_t mix) {
    return static_cast<uint8_t>(
        (static_cast<uint32_t>(idle) * (65535u - mix) +
         static_cast<uint32_t>(effect) * mix + 32767u) /
        65535u);
}

}  // namespace

bool IdleLightingController::read_config(IdleLightingConfig& output) const {
    output = active_;
    return true;
}

IdleLightingStatus IdleLightingController::validate_config(
    const IdleLightingConfig& config) const {
    if (config.idle_brightness_q8 > kEffectUnityGain ||
        config.silence_timeout_ms > kIdleMaximumTimingMs ||
        config.audio_confirmation_ms > kIdleMaximumTimingMs ||
        config.fade_to_effect_ms > kIdleMaximumTimingMs ||
        config.fade_to_idle_ms > kIdleMaximumTimingMs ||
        (config.activity_input_mask & ~kIdleAllInputs) != 0u ||
        (config.strip_enable_mask & ~kIdleAllStrips) != 0u ||
        config.left_activity_floor > kRawPeakMaximum ||
        config.right_activity_floor > kRawPeakMaximum ||
        config.aux_activity_floor > kRawPeakMaximum ||
        config.activity_hysteresis > kRawPeakMaximum) {
        return IdleLightingStatus::invalid_parameter;
    }
    return IdleLightingStatus::ok;
}

IdleLightingStatus IdleLightingController::stage_config(
    const IdleLightingConfig& config) {
    const IdleLightingStatus status = validate_config(config);
    if (status != IdleLightingStatus::ok) {
        return status;
    }
    pending_ = config;
    pending_valid_ = true;
    return IdleLightingStatus::ok;
}

bool IdleLightingController::has_pending_config() const {
    return pending_valid_;
}

bool IdleLightingController::apply_pending_config() {
    if (!pending_valid_) {
        return false;
    }
    const bool reset_runtime = requires_runtime_reset(active_, pending_);
    active_ = pending_;
    pending_valid_ = false;
    if (reset_runtime) {
        runtime_ = {};
    }
    return true;
}

bool IdleLightingController::requires_runtime_reset(
    const IdleLightingConfig& active,
    const IdleLightingConfig& proposed) {
    if (active.enabled != proposed.enabled) {
        return true;
    }
    // A controller with no selected source cannot safely retain a previous
    // activity state. Other controls intentionally preserve the transition.
    return active.activity_input_mask != proposed.activity_input_mask &&
           proposed.activity_input_mask == 0u;
}

const IdleLightingRuntime& IdleLightingController::runtime() const {
    return runtime_;
}

bool IdleLightingController::update_gate(uint16_t raw,
                                         uint16_t floor,
                                         uint16_t hysteresis,
                                         bool& active) {
    const uint16_t close = floor > hysteresis ? floor - hysteresis : 0u;
    active = active ? raw > close : raw > floor;
    return active;
}

uint16_t IdleLightingController::ramp(uint16_t current,
                                      uint16_t target,
                                      uint16_t duration_ms,
                                      uint32_t elapsed_ms) {
    if (current == target || duration_ms == 0u) {
        return target;
    }
    const uint32_t delta = std::min<uint32_t>(
        65535u,
        (static_cast<uint64_t>(elapsed_ms) * 65535u + duration_ms - 1u) /
            duration_ms);
    if (target > current) {
        return static_cast<uint16_t>(std::min<uint32_t>(target, current + delta));
    }
    return static_cast<uint16_t>(current > delta ? current - delta : 0u);
}

RgbwColor IdleLightingController::scale(RgbwColor color, uint16_t level) {
    return {scale_channel(color.red, level),
            scale_channel(color.green, level),
            scale_channel(color.blue, level),
            scale_channel(color.white, level)};
}

RgbwColor IdleLightingController::interpolate(RgbwColor idle,
                                              RgbwColor effect,
                                              uint16_t mix) {
    return {interpolate_channel(idle.red, effect.red, mix),
            interpolate_channel(idle.green, effect.green, mix),
            interpolate_channel(idle.blue, effect.blue, mix),
            interpolate_channel(idle.white, effect.white, mix)};
}

void IdleLightingController::update(const AudioLevelFrame& audio,
                                    uint64_t timestamp_us) {
    if (!runtime_.initialized) {
        runtime_.initialized = true;
        runtime_.last_timestamp_us = timestamp_us;
        runtime_.state = active_.enabled && active_.startup_idle_enabled
                             ? IdleLightingState::idle
                             : IdleLightingState::effects;
        runtime_.effect_mix = runtime_.state == IdleLightingState::idle ? 0u : 65535u;
        runtime_.inactive_since_us = timestamp_us;
        runtime_.inactive_timer_valid = true;
    }

    const uint32_t elapsed_ms = static_cast<uint32_t>(std::min<uint64_t>(
        kMaximumElapsedMs,
        timestamp_us >= runtime_.last_timestamp_us
            ? (timestamp_us - runtime_.last_timestamp_us) / 1000u
            : 0u));
    runtime_.last_timestamp_us = timestamp_us;

    if (!active_.enabled) {
        runtime_.effect_mix = 65535u;
        runtime_.state = IdleLightingState::effects;
        return;
    }

    update_gate(audio.left_peak,
                active_.left_activity_floor,
                active_.activity_hysteresis,
                runtime_.left_active);
    update_gate(audio.right_peak,
                active_.right_activity_floor,
                active_.activity_hysteresis,
                runtime_.right_active);
    update_gate(audio.aux_peak,
                active_.aux_activity_floor,
                active_.activity_hysteresis,
                runtime_.aux_active);

    const bool selected_active =
        ((active_.activity_input_mask & kIdleActivityLeft) != 0u && runtime_.left_active) ||
        ((active_.activity_input_mask & kIdleActivityRight) != 0u && runtime_.right_active) ||
        ((active_.activity_input_mask & kIdleActivityAux) != 0u && runtime_.aux_active);

    bool effects_target = false;
    if (selected_active) {
        runtime_.inactive_since_us = timestamp_us;
        runtime_.inactive_ms = 0u;
        if (!runtime_.active_timer_valid) {
            runtime_.active_since_us = timestamp_us;
            runtime_.active_timer_valid = true;
        }
        effects_target = timestamp_us - runtime_.active_since_us >=
                         static_cast<uint64_t>(active_.audio_confirmation_ms) * 1000u;
    } else {
        runtime_.active_timer_valid = false;
        runtime_.active_since_us = 0u;
        if (!runtime_.inactive_timer_valid) {
            runtime_.inactive_since_us = timestamp_us;
            runtime_.inactive_timer_valid = true;
        }
        runtime_.inactive_ms = static_cast<uint32_t>(std::min<uint64_t>(
            UINT32_MAX, (timestamp_us - runtime_.inactive_since_us) / 1000u));
    }

    const bool idle_target = !selected_active &&
                             runtime_.inactive_ms >= active_.silence_timeout_ms;
    if (effects_target) {
        runtime_.effect_mix = ramp(runtime_.effect_mix,
                                   65535u,
                                   active_.fade_to_effect_ms,
                                   elapsed_ms);
        runtime_.state = runtime_.effect_mix == 65535u
                             ? IdleLightingState::effects
                             : IdleLightingState::fading_to_effects;
    } else if (idle_target) {
        runtime_.effect_mix = ramp(runtime_.effect_mix,
                                   0u,
                                   active_.fade_to_idle_ms,
                                   elapsed_ms);
        runtime_.state = runtime_.effect_mix == 0u
                             ? IdleLightingState::idle
                             : IdleLightingState::fading_to_idle;
    }
}

void IdleLightingController::blend(
    const std::array<EffectRenderSpan, kEffectStripCount>& spans) const {
    if (!active_.enabled || runtime_.effect_mix == 65535u) {
        return;
    }
    const uint16_t brightness = static_cast<uint16_t>(std::min<uint32_t>(
        65535u,
        (static_cast<uint32_t>(active_.idle_brightness_q8) * 65535u +
         kEffectUnityGain / 2u) /
            kEffectUnityGain));
    const RgbwColor idle = scale(active_.idle_color_rgbw, brightness);
    for (std::size_t strip = 0u; strip < spans.size(); ++strip) {
        if ((active_.strip_enable_mask & (1u << strip)) == 0u ||
            spans[strip].pixels == nullptr) {
            continue;
        }
        for (std::size_t pixel = 0u; pixel < spans[strip].pixel_count; ++pixel) {
            spans[strip].pixels[pixel] = interpolate(
                idle, spans[strip].pixels[pixel], runtime_.effect_mix);
        }
    }
}

const char* effect_identifier(EffectType type) {
    const EffectMetadata* const metadata = effect_metadata(type);
    return metadata == nullptr ? "invalid" : metadata->identifier;
}

const char* effect_display_name(EffectType type) {
    const EffectMetadata* const metadata = effect_metadata(type);
    return metadata == nullptr ? "Invalid Effect" : metadata->display_name;
}

bool effect_supports_source(EffectType type, EffectSource source) {
    const EffectMetadata* const metadata = effect_metadata(type);
    return metadata != nullptr &&
           (metadata->source_mask & source_bit(source)) != 0u;
}

const EffectMetadata* effect_metadata(EffectType type) {
    const uint8_t index = static_cast<uint8_t>(type);
    return index < kMetadata.size() ? &kMetadata[index] : nullptr;
}

}  // namespace effects
