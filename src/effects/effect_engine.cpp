#include "effects/effect_engine.hpp"

namespace effects {

namespace {

bool is_valid_effect_type(EffectType type) {
    return type <= EffectType::macro_bands;
}

bool is_valid_effect_source(EffectSource source) {
    return source <= EffectSource::macro_bands;
}

bool is_valid_static_color_mode(StaticColorMode mode) {
    return mode <= StaticColorMode::white_boost;
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

    if (!is_source_compatible(config)) {
        return EffectStatus::incompatible_source;
    }

    if (config.visual_gain > kEffectMaximumGain ||
        config.attack_ms > kEffectMaximumResponseMs ||
        config.release_ms > kEffectMaximumResponseMs) {
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
    std::array<StripEffectConfig, kEffectStripCount> scene{};

    scene[0] = {
        true,
        EffectType::spectrum_bars,
        EffectSource::spectrum_32,
        {0, 0, 255, 0},
        {255, 0, 0, 0},
        {0, 0, 0, 0},
        {{{0, 0, 160, 96}, {0, 200, 255, 0}, {0, 255, 32, 0}, {255, 32, 0, 0}}},
        StaticColorMode::direct_rgbw,
        100u,
        {255, 255, 255, 0},
        false,
        kEffectUnityGain,
        60u,
        180u,
        16u,
        5u,
        4u,
    };
    scene[1] = scene[0];
    scene[1].type = EffectType::mirrored_spectrum_zones;
    scene[1].zone_count = 5u;

    scene[2] = scene[0];
    scene[2].type = EffectType::macro_bands;
    scene[2].source = EffectSource::macro_bands;
    scene[2].primary_color = {255, 0, 0, 0};
    scene[2].secondary_color = {0, 255, 0, 0};
    scene[2].palette = {{{255, 0, 0, 0}, {255, 160, 0, 0},
                         {0, 255, 0, 0}, {0, 0, 255, 0}}};

    scene[3] = scene[0];
    scene[3].type = EffectType::stereo_center_out_vu;
    scene[3].source = EffectSource::stereo_left_right;
    scene[3].primary_color = {0, 255, 0, 0};
    scene[3].secondary_color = {255, 0, 0, 0};
    scene[3].attack_ms = 40u;
    scene[3].release_ms = 140u;

    scene[4] = scene[0];
    scene[4].type = EffectType::scalar_vu;
    scene[4].source = EffectSource::mono;
    scene[4].primary_color = {0, 0, 255, 0};
    scene[4].attack_ms = 40u;
    scene[4].release_ms = 140u;

    scene[5] = scene[4];
    scene[5].source = EffectSource::aux;
    scene[5].primary_color = {255, 255, 255, 0};

    return scene;
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
