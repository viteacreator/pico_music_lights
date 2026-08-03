#include "effects/effect_engine.hpp"
#include "effects/effect_scenes.hpp"

namespace effects {

namespace {

constexpr uint16_t kMaximumAnimationSpeedQ8 = 4096u;
constexpr uint16_t kMaximumColorSpacingQ8 = 4096u;
constexpr uint8_t kMaximumStrobeFrequencyHz = 30u;
constexpr uint16_t kMinimumAdaptiveTriggerPercent = 100u;
constexpr uint16_t kMaximumAdaptiveTriggerPercent = 1000u;

bool is_valid_effect_type(EffectType type) {
    return type <= EffectType::gyver_spectrum_analyzer;
}

bool is_valid_effect_source(EffectSource source) {
    return source <= EffectSource::macro_bands;
}

bool is_valid_static_color_mode(StaticColorMode mode) {
    return mode <= StaticColorMode::white_boost;
}

bool is_valid_vu_color_mode(VuColorMode mode) {
    return mode <= VuColorMode::animated_rainbow;
}

bool is_valid_frequency_selection(FrequencySelection selection) {
    return selection <= FrequencySelection::high;
}

bool is_valid_gyver_full_strip_selection(
    GyverFullStripSelectionPolicy selection) {
    return selection <= GyverFullStripSelectionPolicy::strongest_event;
}

bool is_valid_macro_band_mapping(GenericMacroBandMapping mapping) {
    return mapping <= GenericMacroBandMapping::bass_mid_high;
}

bool is_valid_strobe_envelope_mode(StrobeEnvelopeMode mode) {
    return mode <= StrobeEnvelopeMode::fade_envelope;
}

bool is_gyver_adaptive_frequency_effect(EffectType type) {
    switch (type) {
    case EffectType::gyver_frequency_5_zones:
    case EffectType::gyver_frequency_3_zones:
    case EffectType::gyver_frequency_full_strip:
    case EffectType::gyver_running_frequencies:
        return true;

    default:
        return false;
    }
}

bool is_gyver_auto_gain_effect(EffectType type) {
    return type == EffectType::gyver_vu_gradient ||
           type == EffectType::gyver_vu_rainbow ||
           type == EffectType::gyver_spectrum_analyzer;
}

bool requires_state_reset(const StripEffectConfig& active,
                          const StripEffectConfig& proposed) {
    if ((!active.enabled && proposed.enabled) ||
        active.type != proposed.type ||
        active.source != proposed.source) {
        return true;
    }

    if (active.type == EffectType::spectrum_bars &&
        active.segment_count != proposed.segment_count) {
        return true;
    }

    if (active.type == EffectType::mirrored_spectrum_zones &&
        active.zone_count != proposed.zone_count) {
        return true;
    }

    if (active.type == EffectType::macro_bands &&
        active.macro_region_count != proposed.macro_region_count) {
        return true;
    }

    if ((active.type == EffectType::scalar_vu ||
         active.type == EffectType::stereo_center_out_vu) &&
        active.vu_color_mode != proposed.vu_color_mode) {
        return true;
    }

    if ((active.type == EffectType::one_band_frequency ||
         active.type == EffectType::frequency_comet) &&
        active.frequency_selection != proposed.frequency_selection) {
        return true;
    }

    if (active.type == EffectType::macro_bands &&
        active.macro_band_mapping != proposed.macro_band_mapping) {
        return true;
    }

    if ((active.type == EffectType::gyver_vu_gradient ||
         active.type == EffectType::gyver_vu_rainbow) &&
        (active.gyver_left_noise_floor != proposed.gyver_left_noise_floor ||
         active.gyver_right_noise_floor != proposed.gyver_right_noise_floor ||
         active.gyver_noise_gate_hysteresis !=
             proposed.gyver_noise_gate_hysteresis ||
         active.auto_gain_enabled != proposed.auto_gain_enabled ||
         active.auto_gain_headroom_q8 != proposed.auto_gain_headroom_q8 ||
         active.auto_gain_reference_rise_ms !=
             proposed.auto_gain_reference_rise_ms ||
         active.auto_gain_reference_fall_ms !=
             proposed.auto_gain_reference_fall_ms ||
         active.gyver_rainbow_span_percent !=
             proposed.gyver_rainbow_span_percent)) {
        return true;
    }

    if (active.type == EffectType::gyver_spectrum_analyzer &&
        (active.gyver_spectrum_noise_floor != proposed.gyver_spectrum_noise_floor ||
         active.gyver_spectrum_minimum_peak !=
             proposed.gyver_spectrum_minimum_peak)) {
        return true;
    }

    if (active.type == EffectType::frequency_comet &&
        (active.frequency_comet_tail_percent !=
             proposed.frequency_comet_tail_percent ||
         active.frequency_comet_quiet_threshold !=
             proposed.frequency_comet_quiet_threshold ||
         active.reversed != proposed.reversed)) {
        return true;
    }

    if ((active.type == EffectType::stroboscope ||
         active.type == EffectType::gyver_stroboscope) &&
        (active.strobe_frequency_hz != proposed.strobe_frequency_hz ||
         active.strobe_duty_percent != proposed.strobe_duty_percent ||
         active.strobe_fade_ms != proposed.strobe_fade_ms ||
         active.strobe_envelope_mode != proposed.strobe_envelope_mode)) {
        return true;
    }

    if (active.type == EffectType::gyver_frequency_full_strip &&
        active.gyver_full_strip_selection != proposed.gyver_full_strip_selection) {
        return true;
    }

    if (active.type == EffectType::gyver_running_frequencies &&
        active.gyver_running_frequencies_selection !=
            proposed.gyver_running_frequencies_selection) {
        return true;
    }

    return active.type == EffectType::static_rgbw &&
           active.static_color_mode != proposed.static_color_mode;
}

bool is_scalar_source(EffectSource source) {
    return source >= EffectSource::left && source <= EffectSource::high;
}

bool is_supported_segment_count(uint8_t count) {
    return count == 5u || count == 8u || count == 16u || count == 32u;
}

bool is_source_compatible(const StripEffectConfig& config) {
    switch (config.type) {
    case EffectType::off:
    case EffectType::static_rgbw:
        return config.source == EffectSource::none;

    case EffectType::scalar_vu:
        return is_scalar_source(config.source);

    case EffectType::stereo_center_out_vu:
        return config.source == EffectSource::stereo_left_right;

    case EffectType::spectrum_bars:
    case EffectType::mirrored_spectrum_zones:
        return config.source == EffectSource::spectrum_32;

    case EffectType::macro_bands:
        return config.source == EffectSource::macro_bands;

    case EffectType::one_band_frequency:
    case EffectType::frequency_comet:
        return config.source == EffectSource::macro_bands;

    case EffectType::stroboscope:
    case EffectType::ambient_color_cycle:
    case EffectType::running_rainbow:
        return config.source == EffectSource::none;

    case EffectType::gyver_vu_gradient:
    case EffectType::gyver_vu_rainbow:
        return config.source == EffectSource::stereo_left_right;

    case EffectType::gyver_frequency_5_zones:
    case EffectType::gyver_frequency_3_zones:
    case EffectType::gyver_frequency_full_strip:
    case EffectType::gyver_running_frequencies:
        return config.source == EffectSource::macro_bands;

    case EffectType::gyver_spectrum_analyzer:
        return config.source == EffectSource::spectrum_32;

    case EffectType::gyver_stroboscope:
    case EffectType::gyver_ambient_static:
    case EffectType::gyver_ambient_color_cycle:
    case EffectType::gyver_ambient_running_rainbow:
        return config.source == EffectSource::none;
    }

    return false;
}

}  // namespace

EffectEngine::EffectEngine() {
    const std::array<StripEffectConfig, kEffectStripCount> defaults =
        default_scene();
    for (std::size_t index = 0; index < active_.size(); ++index) {
        active_[index].config = defaults[index];
        reset_state(active_[index].state);
    }

    pending_ = defaults;
    configuration_generation_ = 1u;
}

bool EffectEngine::read_config(std::size_t strip_index,
                               StripEffectConfig& output) const {
    if (strip_index >= active_.size()) {
        return false;
    }

    output = active_[strip_index].config;
    return true;
}

EffectStatus EffectEngine::validate_config(const StripEffectConfig& config) const {
    if (!is_valid_effect_type(config.type)) {
        return EffectStatus::invalid_effect_type;
    }

    if (!is_valid_effect_source(config.source)) {
        return EffectStatus::invalid_source;
    }

    if (!is_valid_static_color_mode(config.static_color_mode)) {
        return EffectStatus::invalid_static_color_mode;
    }

    if (!is_valid_vu_color_mode(config.vu_color_mode) ||
        !is_valid_frequency_selection(config.frequency_selection) ||
        !is_valid_gyver_full_strip_selection(config.gyver_full_strip_selection) ||
        !is_valid_gyver_full_strip_selection(
            config.gyver_running_frequencies_selection) ||
        !is_valid_macro_band_mapping(config.macro_band_mapping) ||
        !is_valid_strobe_envelope_mode(config.strobe_envelope_mode)) {
        return EffectStatus::invalid_parameter;
    }

    if (!is_source_compatible(config)) {
        return EffectStatus::incompatible_source;
    }

    if (config.visual_gain > kEffectMaximumGain ||
        config.attack_ms > kEffectMaximumResponseMs ||
        config.release_ms > kEffectMaximumResponseMs ||
        config.fade_decay_ms > kEffectMaximumResponseMs ||
        config.animation_speed_q8 > kMaximumAnimationSpeedQ8 ||
        config.color_spacing_q8 > kMaximumColorSpacingQ8 ||
        config.strobe_fade_ms > kEffectMaximumResponseMs ||
        config.background_brightness_q8 > kEffectMaximumGain ||
        config.adaptive_fast_response_ms > kEffectMaximumResponseMs ||
        config.adaptive_average_response_ms > kEffectMaximumResponseMs ||
        config.adaptive_event_decay_ms > kEffectMaximumResponseMs ||
        config.gyver_animation_interval_ms > kEffectMaximumResponseMs ||
        config.auto_gain_reference_rise_ms > kEffectMaximumResponseMs ||
        config.auto_gain_reference_fall_ms > kEffectMaximumResponseMs ||
        config.auto_gain_headroom_q8 < kEffectUnityGain ||
        config.auto_gain_headroom_q8 > kEffectMaximumGain ||
        config.gyver_rainbow_span_percent > 400u ||
        config.adaptive_trigger_percent < kMinimumAdaptiveTriggerPercent ||
        config.adaptive_trigger_percent > kMaximumAdaptiveTriggerPercent ||
        config.strobe_duty_percent > 100u ||
        config.frequency_comet_tail_percent > 100u) {
        return EffectStatus::invalid_parameter;
    }

    if ((config.type == EffectType::stroboscope ||
         config.type == EffectType::gyver_stroboscope) &&
        (config.strobe_frequency_hz == 0u ||
         config.strobe_frequency_hz > kMaximumStrobeFrequencyHz)) {
        return EffectStatus::invalid_parameter;
    }

    if (config.type == EffectType::gyver_stroboscope &&
        config.strobe_envelope_mode != StrobeEnvelopeMode::hard_cut) {
        return EffectStatus::invalid_parameter;
    }

    if (config.static_color_mode == StaticColorMode::white_boost &&
        (config.white_drive_percent > 200u ||
         config.rgb_assist_color.white != 0u)) {
        return EffectStatus::invalid_parameter;
    }

    if (config.type == EffectType::spectrum_bars &&
        !is_supported_segment_count(config.segment_count)) {
        return EffectStatus::invalid_parameter;
    }

    if (config.type == EffectType::mirrored_spectrum_zones &&
        (config.zone_count == 0u || config.zone_count > kEffectMaximumMirroredZones)) {
        return EffectStatus::invalid_parameter;
    }

    if (config.type == EffectType::macro_bands &&
        config.macro_region_count != 3u && config.macro_region_count != 4u) {
        return EffectStatus::invalid_parameter;
    }

    if (is_gyver_adaptive_frequency_effect(config.type) &&
        (config.adaptive_fast_response_ms == 0u ||
         config.adaptive_average_response_ms == 0u ||
         config.gyver_animation_interval_ms == 0u)) {
        return EffectStatus::invalid_parameter;
    }

    if (is_gyver_auto_gain_effect(config.type) &&
        (config.auto_gain_reference_rise_ms == 0u ||
         config.auto_gain_reference_fall_ms == 0u)) {
        return EffectStatus::invalid_parameter;
    }

    return EffectStatus::ok;
}

EffectStatus EffectEngine::stage_strip_config(std::size_t strip_index,
                                              const StripEffectConfig& config) {
    if (strip_index >= active_.size()) {
        return EffectStatus::invalid_strip_index;
    }

    const EffectStatus status = validate_config(config);
    if (status != EffectStatus::ok) {
        return status;
    }

    if (!pending_valid_) {
        for (std::size_t index = 0; index < active_.size(); ++index) {
            pending_[index] = active_[index].config;
        }
    }

    pending_[strip_index] = config;
    pending_valid_ = true;
    return EffectStatus::ok;
}

EffectStatus EffectEngine::stage_scene(
    const std::array<StripEffectConfig, kEffectStripCount>& scene) {
    for (const StripEffectConfig& config : scene) {
        const EffectStatus status = validate_config(config);
        if (status != EffectStatus::ok) {
            return status;
        }
    }

    pending_ = scene;
    pending_valid_ = true;
    return EffectStatus::ok;
}

void EffectEngine::restore_default_scene() {
    pending_ = default_scene();
    pending_valid_ = true;
}

bool EffectEngine::has_pending_configuration() const {
    return pending_valid_;
}

bool EffectEngine::apply_pending() {
    if (!pending_valid_) {
        return false;
    }

    for (std::size_t index = 0; index < active_.size(); ++index) {
        if (requires_state_reset(active_[index].config, pending_[index])) {
            reset_state(active_[index].state);
        }

        active_[index].config = pending_[index];
    }

    pending_valid_ = false;
    ++configuration_generation_;
    return true;
}

uint32_t EffectEngine::configuration_generation() const {
    return configuration_generation_;
}

const StripEffectRuntime* EffectEngine::runtime(std::size_t strip_index) const {
    if (strip_index >= active_.size()) {
        return nullptr;
    }

    return &active_[strip_index];
}

void EffectEngine::render(
    const EffectInputSnapshot& snapshot,
    const std::array<EffectRenderSpan, kEffectStripCount>& spans) {
    apply_pending();

    for (std::size_t index = 0; index < active_.size(); ++index) {
        StripEffectRuntime& strip = active_[index];
        const EffectRenderSpan span = spans[index];

        if (!strip.config.enabled || strip.config.type == EffectType::off) {
            clear_span(span);
            continue;
        }

        render_effect(strip, snapshot, span);
    }
}

std::array<StripEffectConfig, kEffectStripCount> EffectEngine::default_scene() {
    return reset_default_scene();
}

void EffectEngine::reset_state(StripEffectState& state) {
    state = {};
}

void EffectEngine::clear_span(EffectRenderSpan span) {
    if (span.pixels == nullptr) {
        return;
    }

    for (std::size_t index = 0; index < span.pixel_count; ++index) {
        span.pixels[index] = {0, 0, 0, 0};
    }
}

}  // namespace effects
