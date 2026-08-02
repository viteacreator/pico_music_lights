#include "effects/effect_engine.hpp"
#include "effects/effect_scenes.hpp"

#include <array>
#include <cstddef>
#include <cstdio>

namespace {

using effects::EffectEngine;
using effects::EffectInputSnapshot;
using effects::EffectRenderSpan;
using effects::EffectSource;
using effects::EffectStatus;
using effects::EffectType;
using effects::FrequencySelection;
using effects::StaticColorMode;
using effects::StripEffectConfig;
using effects::VuColorMode;

constexpr RgbwColor kOff{0, 0, 0, 0};
constexpr RgbwColor kRed{255, 0, 0, 0};
constexpr RgbwColor kGreen{0, 255, 0, 0};
constexpr RgbwColor kBlue{0, 0, 255, 0};
constexpr RgbwColor kWhite{0, 0, 0, 255};

bool colors_equal(RgbwColor first, RgbwColor second) {
    return first.red == second.red && first.green == second.green &&
           first.blue == second.blue && first.white == second.white;
}

bool is_off(RgbwColor color) {
    return colors_equal(color, kOff);
}

bool configurations_equal(const StripEffectConfig& first,
                          const StripEffectConfig& second) {
    bool palettes_equal = true;
    for (std::size_t index = 0u; index < first.palette.size(); ++index) {
        if (!colors_equal(first.palette[index], second.palette[index])) {
            palettes_equal = false;
            break;
        }
    }

    return first.enabled == second.enabled && first.type == second.type &&
           first.source == second.source &&
           colors_equal(first.primary_color, second.primary_color) &&
           colors_equal(first.secondary_color, second.secondary_color) &&
           colors_equal(first.background_color, second.background_color) &&
           palettes_equal &&
           first.static_color_mode == second.static_color_mode &&
           first.white_drive_percent == second.white_drive_percent &&
           colors_equal(first.rgb_assist_color, second.rgb_assist_color) &&
           first.vu_color_mode == second.vu_color_mode &&
           first.frequency_selection == second.frequency_selection &&
           first.animation_speed_q8 == second.animation_speed_q8 &&
           first.color_spacing_q8 == second.color_spacing_q8 &&
           first.fade_decay_ms == second.fade_decay_ms &&
           first.strobe_frequency_hz == second.strobe_frequency_hz &&
           first.strobe_fade_ms == second.strobe_fade_ms &&
           first.reversed == second.reversed &&
           first.visual_gain == second.visual_gain &&
           first.attack_ms == second.attack_ms &&
           first.release_ms == second.release_ms &&
           first.segment_count == second.segment_count &&
           first.zone_count == second.zone_count &&
           first.macro_region_count == second.macro_region_count;
}

std::array<StripEffectConfig, effects::kEffectStripCount> off_scene() {
    std::array<StripEffectConfig, effects::kEffectStripCount> scene{};
    for (StripEffectConfig& config : scene) {
        config.enabled = false;
        config.type = EffectType::off;
        config.source = EffectSource::none;
    }

    return scene;
}

StripEffectConfig scalar_config(EffectSource source, RgbwColor color) {
    StripEffectConfig config{};
    config.enabled = true;
    config.type = EffectType::scalar_vu;
    config.source = source;
    config.primary_color = color;
    config.attack_ms = 0u;
    config.release_ms = 0u;
    return config;
}

EffectInputSnapshot snapshot(const AudioLevelFrame& audio,
                             const SpectrumFrame& spectrum,
                             uint64_t timestamp_us = 33000u) {
    return {&audio, &spectrum, timestamp_us};
}

std::array<EffectRenderSpan, effects::kEffectStripCount> empty_spans() {
    return {};
}

bool test_default_scene_and_configuration_api() {
    EffectEngine engine;
    if (engine.configuration_generation() != 1u ||
        engine.has_pending_configuration()) {
        return false;
    }

    StripEffectConfig canonical{};
    if (!engine.read_config(0u, canonical) || !canonical.enabled ||
        canonical.type != EffectType::stereo_center_out_vu ||
        canonical.source != EffectSource::stereo_left_right ||
        canonical.vu_color_mode != VuColorMode::level_position_gradient ||
        !is_off(canonical.background_color) || canonical.attack_ms == 0u ||
        canonical.release_ms == 0u ||
        !colors_equal(canonical.palette[0], kGreen) ||
        !colors_equal(canonical.palette[1], {255, 255, 0, 0}) ||
        !colors_equal(canonical.palette[2], {255, 128, 0, 0}) ||
        !colors_equal(canonical.palette[3], kRed)) {
        return false;
    }

    for (std::size_t index = 0; index < effects::kEffectStripCount; ++index) {
        StripEffectConfig config{};
        if (!engine.read_config(index, config) ||
            !configurations_equal(config, canonical)) {
            return false;
        }
    }

    StripEffectConfig ignored{};
    return !engine.read_config(effects::kEffectStripCount, ignored) &&
           engine.runtime(effects::kEffectStripCount) == nullptr;
}

bool test_channel_one_has_no_runtime_dependency() {
    EffectEngine engine;
    std::array<StripEffectConfig, effects::kEffectStripCount> before{};
    for (std::size_t index = 0u; index < before.size(); ++index) {
        if (!engine.read_config(index, before[index])) {
            return false;
        }
    }

    StripEffectConfig changed = before[0];
    changed.type = EffectType::scalar_vu;
    changed.source = EffectSource::left;
    changed.vu_color_mode = VuColorMode::solid;
    changed.primary_color = kBlue;
    changed.attack_ms = 0u;
    changed.release_ms = 0u;
    if (engine.stage_strip_config(0u, changed) != EffectStatus::ok ||
        !engine.apply_pending()) {
        return false;
    }

    StripEffectConfig actual{};
    if (!engine.read_config(0u, actual) || !configurations_equal(actual, changed)) {
        return false;
    }

    for (std::size_t index = 1u; index < before.size(); ++index) {
        if (!engine.read_config(index, actual) ||
            !configurations_equal(actual, before[index])) {
            return false;
        }
    }

    return true;
}

bool test_validation_and_atomic_scene_staging() {
    EffectEngine engine;
    StripEffectConfig config = scalar_config(EffectSource::left, kRed);
    if (engine.validate_config(config) != EffectStatus::ok) {
        return false;
    }

    config.source = EffectSource::spectrum_32;
    if (engine.validate_config(config) != EffectStatus::incompatible_source) {
        return false;
    }

    config = scalar_config(EffectSource::left, kRed);
    config.visual_gain = effects::kEffectMaximumGain + 1u;
    if (engine.validate_config(config) != EffectStatus::invalid_parameter) {
        return false;
    }

    config = scalar_config(EffectSource::left, kRed);
    config.type = static_cast<EffectType>(255u);
    if (engine.validate_config(config) != EffectStatus::invalid_effect_type) {
        return false;
    }

    config = {};
    config.enabled = true;
    config.type = EffectType::stroboscope;
    config.source = EffectSource::none;
    if (engine.validate_config(config) != EffectStatus::invalid_parameter) {
        return false;
    }

    config.strobe_frequency_hz = 8u;
    if (engine.validate_config(config) != EffectStatus::ok) {
        return false;
    }

    config = scalar_config(EffectSource::left, kRed);
    if (engine.stage_strip_config(effects::kEffectStripCount, config) !=
        EffectStatus::invalid_strip_index) {
        return false;
    }

    std::array<StripEffectConfig, effects::kEffectStripCount> scene =
        off_scene();
    scene[0] = scalar_config(EffectSource::left, kRed);
    if (engine.stage_scene(scene) != EffectStatus::ok ||
        !engine.has_pending_configuration()) {
        return false;
    }

    StripEffectConfig active{};
    if (!engine.read_config(0u, active) ||
        active.type != EffectType::stereo_center_out_vu) {
        return false;
    }

    const uint32_t generation_before = engine.configuration_generation();
    std::array<StripEffectConfig, effects::kEffectStripCount> invalid_scene = scene;
    invalid_scene[4].type = EffectType::spectrum_bars;
    invalid_scene[4].source = EffectSource::mono;
    if (engine.stage_scene(invalid_scene) != EffectStatus::incompatible_source ||
        !engine.apply_pending() ||
        engine.configuration_generation() != generation_before + 1u) {
        return false;
    }

    if (!engine.read_config(0u, active) || active.type != EffectType::scalar_vu ||
        active.source != EffectSource::left) {
        return false;
    }

    for (std::size_t index = 1u; index < effects::kEffectStripCount; ++index) {
        if (!engine.read_config(index, active) || active.type != EffectType::off) {
            return false;
        }
    }

    return !engine.has_pending_configuration();
}

bool test_off_static_and_span_bounds() {
    EffectEngine engine;
    std::array<StripEffectConfig, effects::kEffectStripCount> scene = off_scene();
    scene[0].enabled = true;
    scene[0].type = EffectType::static_rgbw;
    scene[0].source = EffectSource::none;
    scene[0].primary_color = kWhite;
    if (engine.stage_scene(scene) != EffectStatus::ok) {
        return false;
    }

    std::array<RgbwColor, 5> guarded{{kRed, kOff, kOff, kOff, kGreen}};
    std::array<EffectRenderSpan, effects::kEffectStripCount> spans =
        empty_spans();
    spans[0] = {guarded.data() + 1u, 3u};
    const AudioLevelFrame audio{};
    const SpectrumFrame spectrum{};
    engine.render(snapshot(audio, spectrum), spans);

    if (!colors_equal(guarded[0], kRed) || !colors_equal(guarded[4], kGreen) ||
        guarded[1].white != 255u || guarded[2].white != 255u ||
        guarded[3].white != 255u) {
        return false;
    }

    scene[0].enabled = false;
    if (engine.stage_scene(scene) != EffectStatus::ok) {
        return false;
    }

    engine.render(snapshot(audio, spectrum, 66000u), spans);
    return is_off(guarded[1]) && is_off(guarded[2]) && is_off(guarded[3]) &&
           colors_equal(guarded[0], kRed) && colors_equal(guarded[4], kGreen);
}

bool test_static_white_boost() {
    const AudioLevelFrame audio{};
    const SpectrumFrame spectrum{};

    const auto render_white_boost = [&audio, &spectrum](
                                       uint16_t drive,
                                       RgbwColor assist = {255, 255, 255, 0}) {
        EffectEngine engine;
        std::array<StripEffectConfig, effects::kEffectStripCount> scene = off_scene();
        scene[0].enabled = true;
        scene[0].type = EffectType::static_rgbw;
        scene[0].source = EffectSource::none;
        scene[0].static_color_mode = StaticColorMode::white_boost;
        scene[0].white_drive_percent = drive;
        scene[0].rgb_assist_color = assist;
        std::array<RgbwColor, 1> pixels{};
        std::array<EffectRenderSpan, effects::kEffectStripCount> spans = empty_spans();
        spans[0] = {pixels.data(), pixels.size()};
        if (engine.stage_scene(scene) != EffectStatus::ok) {
            return kOff;
        }

        engine.render(snapshot(audio, spectrum), spans);
        return pixels[0];
    };

    if (!colors_equal(render_white_boost(0u), kOff) ||
        !colors_equal(render_white_boost(50u), {0, 0, 0, 128}) ||
        !colors_equal(render_white_boost(100u), kWhite) ||
        !colors_equal(render_white_boost(150u), {128, 128, 128, 255}) ||
        !colors_equal(render_white_boost(200u), {255, 255, 255, 255})) {
        return false;
    }

    if (!colors_equal(render_white_boost(150u, {200, 100, 50, 0}),
                      {100, 50, 25, 255})) {
        return false;
    }

    StripEffectConfig invalid{};
    invalid.enabled = true;
    invalid.type = EffectType::static_rgbw;
    invalid.source = EffectSource::none;
    invalid.static_color_mode = StaticColorMode::white_boost;
    invalid.white_drive_percent = 201u;
    EffectEngine engine;
    if (engine.validate_config(invalid) != EffectStatus::invalid_parameter) {
        return false;
    }

    invalid.white_drive_percent = 100u;
    invalid.static_color_mode = static_cast<StaticColorMode>(255u);
    if (engine.validate_config(invalid) != EffectStatus::invalid_static_color_mode) {
        return false;
    }

    invalid.static_color_mode = StaticColorMode::white_boost;
    invalid.white_drive_percent = 100u;
    invalid.rgb_assist_color = {255, 255, 255, 1};
    if (engine.validate_config(invalid) != EffectStatus::invalid_parameter) {
        return false;
    }

    invalid.static_color_mode = StaticColorMode::direct_rgbw;
    invalid.primary_color = {8, 16, 32, 64};
    invalid.white_drive_percent = 200u;
    std::array<StripEffectConfig, effects::kEffectStripCount> scene = off_scene();
    scene[0] = invalid;
    std::array<RgbwColor, 1> pixels{};
    std::array<EffectRenderSpan, effects::kEffectStripCount> spans = empty_spans();
    spans[0] = {pixels.data(), pixels.size()};
    if (engine.stage_scene(scene) != EffectStatus::ok) {
        return false;
    }

    engine.render(snapshot(audio, spectrum), spans);
    return colors_equal(pixels[0], invalid.primary_color);
}

bool state_resets_for_change(StripEffectConfig initial,
                             StripEffectConfig changed) {
    EffectEngine engine;
    std::array<StripEffectConfig, effects::kEffectStripCount> scene = off_scene();
    scene[0] = initial;
    if (engine.stage_scene(scene) != EffectStatus::ok) {
        return false;
    }

    AudioLevelFrame audio{};
    audio.left_peak = 2047u;
    SpectrumFrame spectrum{};
    spectrum.raw_bands.fill(65535u);
    std::array<RgbwColor, 8> pixels{};
    std::array<EffectRenderSpan, effects::kEffectStripCount> spans = empty_spans();
    spans[0] = {pixels.data(), pixels.size()};
    engine.render(snapshot(audio, spectrum), spans);
    if (!engine.runtime(0u)->state.initialized ||
        engine.stage_strip_config(0u, changed) != EffectStatus::ok ||
        !engine.apply_pending()) {
        return false;
    }

    return !engine.runtime(0u)->state.initialized;
}

bool test_state_reset_compatibility() {
    StripEffectConfig scalar = scalar_config(EffectSource::left, kRed);
    StripEffectConfig source_changed = scalar;
    source_changed.source = EffectSource::right;
    if (!state_resets_for_change(scalar, source_changed)) {
        return false;
    }

    StripEffectConfig spectrum{};
    spectrum.enabled = true;
    spectrum.type = EffectType::spectrum_bars;
    spectrum.source = EffectSource::spectrum_32;
    spectrum.segment_count = 8u;
    StripEffectConfig spectrum_changed = spectrum;
    spectrum_changed.segment_count = 16u;
    if (!state_resets_for_change(spectrum, spectrum_changed)) {
        return false;
    }

    StripEffectConfig mirrored = spectrum;
    mirrored.type = EffectType::mirrored_spectrum_zones;
    mirrored.zone_count = 5u;
    StripEffectConfig mirrored_changed = mirrored;
    mirrored_changed.zone_count = 6u;
    if (!state_resets_for_change(mirrored, mirrored_changed)) {
        return false;
    }

    StripEffectConfig macro = spectrum;
    macro.type = EffectType::macro_bands;
    macro.source = EffectSource::macro_bands;
    macro.macro_region_count = 3u;
    StripEffectConfig macro_changed = macro;
    macro_changed.macro_region_count = 4u;
    if (!state_resets_for_change(macro, macro_changed)) {
        return false;
    }

    StripEffectConfig disabled = scalar;
    disabled.enabled = false;
    StripEffectConfig enabled = disabled;
    enabled.enabled = true;
    EffectEngine engine;
    std::array<StripEffectConfig, effects::kEffectStripCount> scene = off_scene();
    scene[0] = disabled;
    if (engine.stage_scene(scene) != EffectStatus::ok ||
        !engine.apply_pending() ||
        engine.stage_strip_config(0u, enabled) != EffectStatus::ok ||
        !engine.apply_pending()) {
        return false;
    }

    return !engine.runtime(0u)->state.initialized;
}

bool test_scalar_sources_and_reverse_direction() {
    EffectEngine engine;
    std::array<StripEffectConfig, effects::kEffectStripCount> scene = off_scene();
    scene[0] = scalar_config(EffectSource::left, kRed);
    scene[1] = scalar_config(EffectSource::aux, kGreen);
    scene[1].reversed = true;
    if (engine.stage_scene(scene) != EffectStatus::ok) {
        return false;
    }

    AudioLevelFrame audio{};
    audio.left_peak = 1024u;
    audio.aux_peak = 1024u;
    SpectrumFrame spectrum{};
    std::array<RgbwColor, 4> left{};
    std::array<RgbwColor, 4> aux{};
    std::array<EffectRenderSpan, effects::kEffectStripCount> spans = empty_spans();
    spans[0] = {left.data(), left.size()};
    spans[1] = {aux.data(), aux.size()};
    engine.render(snapshot(audio, spectrum), spans);

    return left[0].red == 255u && left[1].red == 255u && is_off(left[2]) &&
           is_off(left[3]) && is_off(aux[0]) && is_off(aux[1]) &&
           aux[2].green == 255u && aux[3].green == 255u;
}

bool test_state_isolation_and_effect_reset() {
    EffectEngine engine;
    std::array<StripEffectConfig, effects::kEffectStripCount> scene = off_scene();
    scene[0] = scalar_config(EffectSource::left, kRed);
    scene[0].attack_ms = 100u;
    scene[0].release_ms = 100u;
    scene[1] = scalar_config(EffectSource::right, kBlue);
    scene[1].attack_ms = 100u;
    scene[1].release_ms = 100u;
    if (engine.stage_scene(scene) != EffectStatus::ok) {
        return false;
    }

    AudioLevelFrame audio{};
    audio.left_peak = 2047u;
    audio.right_peak = 1024u;
    SpectrumFrame spectrum{};
    std::array<RgbwColor, 8> first{};
    std::array<RgbwColor, 8> second{};
    std::array<EffectRenderSpan, effects::kEffectStripCount> spans = empty_spans();
    spans[0] = {first.data(), first.size()};
    spans[1] = {second.data(), second.size()};
    engine.render(snapshot(audio, spectrum), spans);

    const effects::StripEffectRuntime* first_runtime = engine.runtime(0u);
    const effects::StripEffectRuntime* second_runtime = engine.runtime(1u);
    if (first_runtime == nullptr || second_runtime == nullptr ||
        first_runtime->state.smoothed_levels[0] == 0u ||
        second_runtime->state.smoothed_levels[0] == 0u) {
        return false;
    }

    StripEffectConfig changed = scene[0];
    changed.primary_color = kGreen;
    if (engine.stage_strip_config(0u, changed) != EffectStatus::ok ||
        !engine.apply_pending()) {
        return false;
    }

    first_runtime = engine.runtime(0u);
    second_runtime = engine.runtime(1u);
    if (first_runtime == nullptr || second_runtime == nullptr ||
        first_runtime->state.smoothed_levels[0] == 0u ||
        second_runtime->state.smoothed_levels[0] == 0u) {
        return false;
    }

    changed.type = EffectType::static_rgbw;
    changed.source = EffectSource::none;
    if (engine.stage_strip_config(0u, changed) != EffectStatus::ok ||
        !engine.apply_pending()) {
        return false;
    }

    first_runtime = engine.runtime(0u);
    second_runtime = engine.runtime(1u);
    return first_runtime != nullptr && second_runtime != nullptr &&
           !first_runtime->state.initialized &&
           first_runtime->state.smoothed_levels[0] == 0u &&
           second_runtime->state.smoothed_levels[0] != 0u;
}

bool test_spectrum_and_macro_routing() {
    const AudioLevelFrame audio{};
    SpectrumFrame spectrum{};
    spectrum.raw_bands.fill(65535u);
    spectrum.raw_bass = 65535u;
    spectrum.raw_low = 32768u;
    spectrum.raw_mid = 16384u;
    spectrum.raw_high = 8192u;

    for (uint8_t segments : {5u, 8u, 16u, 32u}) {
        EffectEngine engine;
        std::array<StripEffectConfig, effects::kEffectStripCount> scene = off_scene();
        scene[0].enabled = true;
        scene[0].type = EffectType::spectrum_bars;
        scene[0].source = EffectSource::spectrum_32;
        scene[0].segment_count = segments;
        scene[0].palette = {kRed, kGreen, kBlue, kWhite};
        if (engine.stage_scene(scene) != EffectStatus::ok) {
            return false;
        }

        std::array<RgbwColor, 17> pixels{};
        std::array<EffectRenderSpan, effects::kEffectStripCount> spans = empty_spans();
        spans[0] = {pixels.data(), pixels.size()};
        engine.render(snapshot(audio, spectrum), spans);
        if (is_off(pixels[0]) || is_off(pixels[8]) || is_off(pixels[16])) {
            return false;
        }
    }

    EffectEngine macro_engine;
    std::array<StripEffectConfig, effects::kEffectStripCount> scene = off_scene();
    scene[0].enabled = true;
    scene[0].type = EffectType::macro_bands;
    scene[0].source = EffectSource::macro_bands;
    scene[0].macro_region_count = 4u;
    scene[0].palette = {kRed, kGreen, kBlue, kWhite};
    if (macro_engine.stage_scene(scene) != EffectStatus::ok) {
        return false;
    }

    std::array<RgbwColor, 10> pixels{};
    std::array<EffectRenderSpan, effects::kEffectStripCount> spans = empty_spans();
    spans[0] = {pixels.data(), pixels.size()};
    macro_engine.render(snapshot(audio, spectrum), spans);
    return pixels[0].red != 0u && pixels[3].green != 0u &&
           pixels[5].blue != 0u && pixels[8].white != 0u;
}

bool test_stereo_mirrored_macro_and_response_geometry() {
    AudioLevelFrame audio{};
    audio.left_peak = 2047u;
    audio.right_peak = 2047u;
    SpectrumFrame spectrum{};
    spectrum.raw_bands.fill(65535u);
    spectrum.raw_bass = 65535u;
    spectrum.raw_low = 32768u;
    spectrum.raw_mid = 65535u;
    spectrum.raw_high = 16384u;

    EffectEngine stereo_engine;
    std::array<StripEffectConfig, effects::kEffectStripCount> scene = off_scene();
    scene[0].enabled = true;
    scene[0].type = EffectType::stereo_center_out_vu;
    scene[0].source = EffectSource::stereo_left_right;
    scene[0].primary_color = kRed;
    scene[0].secondary_color = kBlue;
    if (stereo_engine.stage_scene(scene) != EffectStatus::ok) {
        return false;
    }

    std::array<RgbwColor, 5> stereo_pixels{};
    std::array<EffectRenderSpan, effects::kEffectStripCount> spans = empty_spans();
    spans[0] = {stereo_pixels.data(), stereo_pixels.size()};
    stereo_engine.render(snapshot(audio, spectrum), spans);
    if (stereo_pixels[0].red != 255u || stereo_pixels[2].red != 255u ||
        stereo_pixels[3].blue != 255u || stereo_pixels[4].blue != 255u) {
        return false;
    }

    EffectEngine mirrored_engine;
    scene = off_scene();
    scene[0].enabled = true;
    scene[0].type = EffectType::mirrored_spectrum_zones;
    scene[0].source = EffectSource::spectrum_32;
    scene[0].zone_count = 5u;
    scene[0].palette = {kRed, kGreen, kBlue, kWhite};
    if (mirrored_engine.stage_scene(scene) != EffectStatus::ok) {
        return false;
    }

    std::array<RgbwColor, 10> mirrored_pixels{};
    spans = empty_spans();
    spans[0] = {mirrored_pixels.data(), mirrored_pixels.size()};
    mirrored_engine.render(snapshot(audio, spectrum), spans);
    if (!colors_equal(mirrored_pixels[0], mirrored_pixels[9]) ||
        !colors_equal(mirrored_pixels[2], mirrored_pixels[7])) {
        return false;
    }

    EffectEngine macro_engine;
    scene = off_scene();
    scene[0].enabled = true;
    scene[0].type = EffectType::macro_bands;
    scene[0].source = EffectSource::macro_bands;
    scene[0].macro_region_count = 3u;
    scene[0].palette = {kRed, kGreen, kBlue, kWhite};
    if (macro_engine.stage_scene(scene) != EffectStatus::ok) {
        return false;
    }

    std::array<RgbwColor, 10> macro_pixels{};
    spans = empty_spans();
    spans[0] = {macro_pixels.data(), macro_pixels.size()};
    macro_engine.render(snapshot(audio, spectrum), spans);
    if (macro_pixels[0].red == 0u || macro_pixels[4].green == 0u ||
        macro_pixels[7].blue == 0u || macro_pixels[9].blue == 0u) {
        return false;
    }

    EffectEngine response_engine;
    scene = off_scene();
    scene[0] = scalar_config(EffectSource::left, kRed);
    scene[0].attack_ms = 100u;
    scene[0].release_ms = 100u;
    if (response_engine.stage_scene(scene) != EffectStatus::ok) {
        return false;
    }

    std::array<RgbwColor, 8> response_pixels{};
    spans = empty_spans();
    spans[0] = {response_pixels.data(), response_pixels.size()};
    response_engine.render(snapshot(audio, spectrum, 33000u), spans);
    audio.left_peak = 0u;
    response_engine.render(snapshot(audio, spectrum, 66000u), spans);
    const uint16_t normal_release =
        response_engine.runtime(0u)->state.smoothed_levels[0];
    response_engine.render(snapshot(audio, spectrum, 266000u), spans);
    const uint16_t delayed_release =
        response_engine.runtime(0u)->state.smoothed_levels[0];
    return normal_release != 0u && delayed_release < normal_release;
}

bool test_vu_colour_modes_and_centre_geometry() {
    AudioLevelFrame audio{};
    audio.left_peak = 2047u;
    audio.right_peak = 2047u;
    SpectrumFrame spectrum{};

    std::array<StripEffectConfig, effects::kEffectStripCount> scene = off_scene();
    scene[0].enabled = true;
    scene[0].type = EffectType::stereo_center_out_vu;
    scene[0].source = EffectSource::stereo_left_right;
    scene[0].attack_ms = 0u;
    scene[0].release_ms = 0u;
    scene[0].vu_color_mode = VuColorMode::level_position_gradient;
    scene[0].palette = {kGreen, {255, 255, 0, 0}, {255, 128, 0, 0}, kRed};

    EffectEngine gradient_engine;
    if (gradient_engine.stage_scene(scene) != EffectStatus::ok) {
        return false;
    }

    std::array<RgbwColor, 5> odd_pixels{};
    std::array<EffectRenderSpan, effects::kEffectStripCount> spans = empty_spans();
    spans[0] = {odd_pixels.data(), odd_pixels.size()};
    gradient_engine.render(snapshot(audio, spectrum), spans);
    if (!colors_equal(odd_pixels[2], kGreen) ||
        odd_pixels[1].red != 255u || odd_pixels[1].green <= 128u ||
        odd_pixels[1].green >= 255u || odd_pixels[1].blue != 0u ||
        !colors_equal(odd_pixels[0], kRed) ||
        !colors_equal(odd_pixels[3], kGreen) ||
        !colors_equal(odd_pixels[4], kRed)) {
        return false;
    }

    EffectEngine even_engine;
    if (even_engine.stage_scene(scene) != EffectStatus::ok) {
        return false;
    }

    std::array<RgbwColor, 6> even_pixels{};
    spans = empty_spans();
    spans[0] = {even_pixels.data(), even_pixels.size()};
    even_engine.render(snapshot(audio, spectrum), spans);
    if (!colors_equal(even_pixels[2], kGreen) ||
        !colors_equal(even_pixels[3], kGreen) ||
        !colors_equal(even_pixels[0], kRed) ||
        !colors_equal(even_pixels[5], kRed)) {
        return false;
    }

    scene[0].type = EffectType::scalar_vu;
    scene[0].source = EffectSource::left;
    scene[0].vu_color_mode = VuColorMode::solid;
    scene[0].primary_color = kBlue;
    EffectEngine solid_engine;
    if (solid_engine.stage_scene(scene) != EffectStatus::ok) {
        return false;
    }

    std::array<RgbwColor, 4> solid_pixels{};
    spans = empty_spans();
    spans[0] = {solid_pixels.data(), solid_pixels.size()};
    solid_engine.render(snapshot(audio, spectrum), spans);
    for (const RgbwColor pixel : solid_pixels) {
        if (!colors_equal(pixel, kBlue)) {
            return false;
        }
    }

    scene[0].vu_color_mode = VuColorMode::animated_rainbow;
    scene[0].animation_speed_q8 = 256u;
    scene[0].color_spacing_q8 = 256u;
    EffectEngine rainbow_engine;
    if (rainbow_engine.stage_scene(scene) != EffectStatus::ok) {
        return false;
    }

    std::array<RgbwColor, 4> first_rainbow{};
    std::array<RgbwColor, 4> second_rainbow{};
    spans = empty_spans();
    spans[0] = {first_rainbow.data(), first_rainbow.size()};
    rainbow_engine.render(snapshot(audio, spectrum, 33000u), spans);
    spans[0] = {second_rainbow.data(), second_rainbow.size()};
    rainbow_engine.render(snapshot(audio, spectrum, 66000u), spans);
    return !colors_equal(first_rainbow[0], first_rainbow[1]) &&
           !colors_equal(first_rainbow[0], second_rainbow[0]);
}

bool any_lit(const std::array<RgbwColor, 8>& pixels) {
    for (const RgbwColor pixel : pixels) {
        if (!is_off(pixel)) {
            return true;
        }
    }

    return false;
}

bool test_catalog_dynamic_effects_and_independent_state() {
    AudioLevelFrame audio{};
    SpectrumFrame spectrum{};
    spectrum.raw_low = 65535u;
    spectrum.raw_mid = 32768u;
    spectrum.raw_high = 16384u;

    std::array<StripEffectConfig, effects::kEffectStripCount> scene =
        off_scene();
    scene[0].enabled = true;
    scene[0].type = EffectType::one_band_frequency;
    scene[0].source = EffectSource::macro_bands;
    scene[0].frequency_selection = FrequencySelection::low;
    scene[0].primary_color = kRed;

    scene[1].enabled = true;
    scene[1].type = EffectType::stroboscope;
    scene[1].source = EffectSource::none;
    scene[1].frequency_selection = FrequencySelection::low;
    scene[1].primary_color = kGreen;
    scene[1].strobe_frequency_hz = 8u;
    scene[1].strobe_fade_ms = 100u;

    scene[2].enabled = true;
    scene[2].type = EffectType::ambient_color_cycle;
    scene[2].source = EffectSource::none;

    scene[3].enabled = true;
    scene[3].type = EffectType::running_rainbow;
    scene[3].source = EffectSource::none;
    scene[3].color_spacing_q8 = 256u;

    scene[4].enabled = true;
    scene[4].type = EffectType::running_frequency;
    scene[4].source = EffectSource::macro_bands;
    scene[4].frequency_selection = FrequencySelection::high;
    scene[4].primary_color = kBlue;
    scene[4].fade_decay_ms = 180u;

    scene[5].enabled = true;
    scene[5].type = EffectType::static_rgbw;
    scene[5].source = EffectSource::none;
    scene[5].primary_color = kWhite;

    EffectEngine engine;
    if (engine.stage_scene(scene) != EffectStatus::ok) {
        return false;
    }

    std::array<std::array<RgbwColor, 8>, effects::kEffectStripCount> pixels{};
    std::array<EffectRenderSpan, effects::kEffectStripCount> spans{};
    for (std::size_t index = 0u; index < spans.size(); ++index) {
        spans[index] = {pixels[index].data(), pixels[index].size()};
    }

    engine.render(snapshot(audio, spectrum, 33000u), spans);
    if (!colors_equal(pixels[0][0], kRed) || !colors_equal(pixels[0][7], kRed) ||
        !any_lit(pixels[1]) || !any_lit(pixels[2]) || !any_lit(pixels[3]) ||
        !any_lit(pixels[4]) || !colors_equal(pixels[5][0], kWhite)) {
        return false;
    }

    if (colors_equal(pixels[3][0], pixels[3][7])) {
        return false;
    }

    std::array<StripEffectConfig, effects::kEffectStripCount> independent =
        off_scene();
    for (std::size_t index = 0u; index < independent.size(); ++index) {
        independent[index] = scene[2];
        independent[index].animation_speed_q8 = static_cast<uint16_t>(
            (index + 1u) * 256u);
    }

    EffectEngine independent_engine;
    if (independent_engine.stage_scene(independent) != EffectStatus::ok) {
        return false;
    }

    independent_engine.render(snapshot(audio, spectrum, 33000u), spans);
    for (std::size_t index = 0u; index < independent.size(); ++index) {
        const effects::StripEffectRuntime* runtime =
            independent_engine.runtime(index);
        const uint16_t expected_phase = static_cast<uint16_t>(
            (index + 1u) * 33u);
        if (runtime == nullptr ||
            runtime->state.animation_phase != expected_phase) {
            return false;
        }
    }

    StripEffectConfig invalid = scene[0];
    invalid.source = EffectSource::left;
    return engine.validate_config(invalid) == EffectStatus::incompatible_source;
}

bool test_running_rainbow_uses_per_pixel_spacing() {
    const AudioLevelFrame audio{};
    const SpectrumFrame spectrum{};
    std::array<StripEffectConfig, effects::kEffectStripCount> scene =
        off_scene();
    scene[0].enabled = true;
    scene[0].type = EffectType::running_rainbow;
    scene[0].source = EffectSource::none;
    scene[0].animation_speed_q8 = 0u;
    scene[0].color_spacing_q8 = 256u;

    EffectEngine engine;
    if (engine.stage_scene(scene) != EffectStatus::ok) {
        return false;
    }

    std::array<RgbwColor, 4> forward{};
    std::array<EffectRenderSpan, effects::kEffectStripCount> spans =
        empty_spans();
    spans[0] = {forward.data(), forward.size()};
    engine.render(snapshot(audio, spectrum), spans);
    if (!colors_equal(forward[0], {255u, 0u, 0u, 0u}) ||
        !colors_equal(forward[1], {255u, 1u, 0u, 0u}) ||
        !colors_equal(forward[2], {255u, 2u, 0u, 0u}) ||
        !colors_equal(forward[3], {255u, 3u, 0u, 0u})) {
        return false;
    }

    scene[0].reversed = true;
    if (engine.stage_scene(scene) != EffectStatus::ok) {
        return false;
    }

    std::array<RgbwColor, 4> reversed{};
    spans[0] = {reversed.data(), reversed.size()};
    engine.render(snapshot(audio, spectrum, 66000u), spans);
    return colors_equal(reversed[0], {255u, 3u, 0u, 0u}) &&
           colors_equal(reversed[1], {255u, 2u, 0u, 0u}) &&
           colors_equal(reversed[2], {255u, 1u, 0u, 0u}) &&
           colors_equal(reversed[3], {255u, 0u, 0u, 0u});
}

bool smoothed_state_matches_across_lengths(StripEffectConfig config,
                                            const AudioLevelFrame& audio,
                                            const SpectrumFrame& spectrum,
                                            std::size_t state_count) {
    std::array<StripEffectConfig, effects::kEffectStripCount> scene =
        off_scene();
    scene[0] = config;

    EffectEngine short_engine;
    EffectEngine long_engine;
    if (short_engine.stage_scene(scene) != EffectStatus::ok ||
        long_engine.stage_scene(scene) != EffectStatus::ok) {
        return false;
    }

    std::array<RgbwColor, 5> short_pixels{};
    std::array<RgbwColor, 65> long_pixels{};
    std::array<EffectRenderSpan, effects::kEffectStripCount> short_spans =
        empty_spans();
    std::array<EffectRenderSpan, effects::kEffectStripCount> long_spans =
        empty_spans();
    short_spans[0] = {short_pixels.data(), short_pixels.size()};
    long_spans[0] = {long_pixels.data(), long_pixels.size()};
    short_engine.render(snapshot(audio, spectrum), short_spans);
    long_engine.render(snapshot(audio, spectrum), long_spans);

    const effects::StripEffectRuntime* short_runtime = short_engine.runtime(0u);
    const effects::StripEffectRuntime* long_runtime = long_engine.runtime(0u);
    if (short_runtime == nullptr || long_runtime == nullptr) {
        return false;
    }

    for (std::size_t index = 0u; index < state_count; ++index) {
        if (short_runtime->state.smoothed_levels[index] == 0u ||
            short_runtime->state.smoothed_levels[index] !=
                long_runtime->state.smoothed_levels[index]) {
            return false;
        }
    }

    return true;
}

bool test_segment_smoothing_is_once_per_frame_and_converges() {
    const AudioLevelFrame audio{};
    SpectrumFrame spectrum{};
    spectrum.raw_bands.fill(1u);
    spectrum.raw_bass = 1u;
    spectrum.raw_low = 1u;
    spectrum.raw_mid = 1u;
    spectrum.raw_high = 1u;

    StripEffectConfig spectrum_bars{};
    spectrum_bars.enabled = true;
    spectrum_bars.type = EffectType::spectrum_bars;
    spectrum_bars.source = EffectSource::spectrum_32;
    spectrum_bars.segment_count = 5u;
    spectrum_bars.attack_ms = 5000u;
    spectrum_bars.release_ms = 5000u;
    spectrum_bars.palette = {kRed, kGreen, kBlue, kWhite};
    if (!smoothed_state_matches_across_lengths(spectrum_bars,
                                                audio,
                                                spectrum,
                                                spectrum_bars.segment_count)) {
        return false;
    }

    StripEffectConfig mirrored{};
    mirrored.enabled = true;
    mirrored.type = EffectType::mirrored_spectrum_zones;
    mirrored.source = EffectSource::spectrum_32;
    mirrored.zone_count = 5u;
    mirrored.attack_ms = 5000u;
    mirrored.release_ms = 5000u;
    mirrored.palette = {kRed, kGreen, kBlue, kWhite};
    if (!smoothed_state_matches_across_lengths(mirrored,
                                                audio,
                                                spectrum,
                                                mirrored.zone_count)) {
        return false;
    }

    StripEffectConfig macro{};
    macro.enabled = true;
    macro.type = EffectType::macro_bands;
    macro.source = EffectSource::macro_bands;
    macro.macro_region_count = 4u;
    macro.attack_ms = 5000u;
    macro.release_ms = 5000u;
    macro.palette = {kRed, kGreen, kBlue, kWhite};
    if (!smoothed_state_matches_across_lengths(macro,
                                                audio,
                                                spectrum,
                                                macro.macro_region_count)) {
        return false;
    }

    EffectEngine convergence_engine;
    std::array<StripEffectConfig, effects::kEffectStripCount> scene =
        off_scene();
    scene[0] = spectrum_bars;
    if (convergence_engine.stage_scene(scene) != EffectStatus::ok) {
        return false;
    }

    std::array<RgbwColor, 5> pixels{};
    std::array<EffectRenderSpan, effects::kEffectStripCount> spans =
        empty_spans();
    spans[0] = {pixels.data(), pixels.size()};
    convergence_engine.render(snapshot(audio, spectrum), spans);
    if (convergence_engine.runtime(0u)->state.smoothed_levels[0] != 1u) {
        return false;
    }

    spectrum.raw_bands.fill(0u);
    convergence_engine.render(snapshot(audio, spectrum, 66000u), spans);
    return convergence_engine.runtime(0u)->state.smoothed_levels[0] == 0u;
}

bool test_all_effects_handle_short_spans_without_out_of_bounds_writes() {
    AudioLevelFrame audio{};
    audio.left_peak = 2047u;
    audio.right_peak = 2047u;
    SpectrumFrame spectrum{};
    spectrum.raw_bands.fill(65535u);
    spectrum.raw_bass = 65535u;
    spectrum.raw_low = 65535u;
    spectrum.raw_mid = 65535u;
    spectrum.raw_high = 65535u;

    std::array<StripEffectConfig, 12> configurations{};
    configurations[0] = {true, EffectType::static_rgbw, EffectSource::none};
    configurations[0].primary_color = kWhite;
    configurations[1] = scalar_config(EffectSource::left, kRed);
    configurations[2].enabled = true;
    configurations[2].type = EffectType::stereo_center_out_vu;
    configurations[2].source = EffectSource::stereo_left_right;
    configurations[2].primary_color = kRed;
    configurations[2].secondary_color = kBlue;
    configurations[3].enabled = true;
    configurations[3].type = EffectType::spectrum_bars;
    configurations[3].source = EffectSource::spectrum_32;
    configurations[3].segment_count = 5u;
    configurations[4] = configurations[3];
    configurations[4].type = EffectType::mirrored_spectrum_zones;
    configurations[4].zone_count = 5u;
    configurations[5].enabled = true;
    configurations[5].type = EffectType::macro_bands;
    configurations[5].source = EffectSource::macro_bands;
    configurations[5].macro_region_count = 4u;
    configurations[6].enabled = true;
    configurations[6].type = EffectType::one_band_frequency;
    configurations[6].source = EffectSource::macro_bands;
    configurations[7].enabled = true;
    configurations[7].type = EffectType::stroboscope;
    configurations[7].source = EffectSource::none;
    configurations[8].enabled = true;
    configurations[8].type = EffectType::ambient_color_cycle;
    configurations[8].source = EffectSource::none;
    configurations[9].enabled = true;
    configurations[9].type = EffectType::running_rainbow;
    configurations[9].source = EffectSource::none;
    configurations[10].enabled = true;
    configurations[10].type = EffectType::running_frequency;
    configurations[10].source = EffectSource::macro_bands;
    configurations[11].enabled = true;
    configurations[11].type = EffectType::off;
    configurations[11].source = EffectSource::none;

    for (const StripEffectConfig& config : configurations) {
        EffectEngine engine;
        std::array<StripEffectConfig, effects::kEffectStripCount> scene =
            off_scene();
        scene[0] = config;
        if (engine.stage_scene(scene) != EffectStatus::ok) {
            return false;
        }

        std::array<EffectRenderSpan, effects::kEffectStripCount> spans =
            empty_spans();
        spans[0] = {nullptr, 0u};
        engine.render(snapshot(audio, spectrum), spans);

        std::array<RgbwColor, 4> guarded{{kRed, kOff, kOff, kGreen}};
        spans[0] = {guarded.data() + 1u, 2u};
        engine.render(snapshot(audio, spectrum, 66000u), spans);
        if (!colors_equal(guarded.front(), kRed) ||
            !colors_equal(guarded.back(), kGreen)) {
            return false;
        }
    }

    return true;
}

bool test_reset_default_and_diagnostic_scene_data() {
    const std::array<StripEffectConfig, effects::kEffectStripCount> reset =
        effects::reset_default_scene();
    for (const StripEffectConfig& config : reset) {
        if (!config.enabled ||
            config.type != EffectType::stereo_center_out_vu ||
            config.source != EffectSource::stereo_left_right ||
            config.vu_color_mode != VuColorMode::level_position_gradient ||
            !is_off(config.background_color) ||
            !colors_equal(config.palette[0], kGreen) ||
            !colors_equal(config.palette[3], kRed)) {
            return false;
        }
    }

    const std::array<StripEffectConfig, effects::kEffectStripCount> first =
        effects::diagnostic_scene(effects::DiagnosticSceneId::vu_and_ambient);
    const std::array<StripEffectConfig, effects::kEffectStripCount> second =
        effects::diagnostic_scene(
            effects::DiagnosticSceneId::spectrum_and_motion);
    if (first[0].type != EffectType::scalar_vu ||
        first[1].vu_color_mode != VuColorMode::animated_rainbow ||
        first[2].type != EffectType::static_rgbw ||
        first[3].type != EffectType::stroboscope ||
        first[4].type != EffectType::ambient_color_cycle ||
        first[5].type != EffectType::running_rainbow ||
        second[0].type != EffectType::spectrum_bars ||
        second[1].type != EffectType::mirrored_spectrum_zones ||
        second[2].type != EffectType::macro_bands ||
        second[3].type != EffectType::one_band_frequency ||
        second[4].type != EffectType::running_frequency ||
        second[5].static_color_mode != StaticColorMode::white_boost ||
        effects::diagnostic_scene_duration_ms(
            effects::DiagnosticSceneId::vu_and_ambient) == 0u ||
        effects::next_diagnostic_scene(
            effects::DiagnosticSceneId::vu_and_ambient) !=
            effects::DiagnosticSceneId::spectrum_and_motion ||
        effects::next_diagnostic_scene(
            effects::DiagnosticSceneId::spectrum_and_motion) !=
            effects::DiagnosticSceneId::vu_and_ambient) {
        return false;
    }

    EffectEngine engine;
    const uint32_t generation_before = engine.configuration_generation();
    if (engine.stage_scene(first) != EffectStatus::ok ||
        !engine.has_pending_configuration()) {
        return false;
    }

    const AudioLevelFrame audio{};
    const SpectrumFrame spectrum{};
    engine.render(snapshot(audio, spectrum), empty_spans());
    if (engine.configuration_generation() != generation_before + 1u ||
        engine.has_pending_configuration()) {
        return false;
    }

    if (engine.stage_scene(second) != EffectStatus::ok) {
        return false;
    }

    engine.render(snapshot(audio, spectrum, 66000u), empty_spans());
    return engine.configuration_generation() == generation_before + 2u;
}

struct NamedTest {
    const char* name;
    bool (*function)();
};

}  // namespace

int main() {
    constexpr std::array<NamedTest, 16> kTests{{
        {"default scene and configuration API", test_default_scene_and_configuration_api},
        {"channel one has no runtime dependency", test_channel_one_has_no_runtime_dependency},
        {"validation and atomic scene staging", test_validation_and_atomic_scene_staging},
        {"off, static RGBW, and span bounds", test_off_static_and_span_bounds},
        {"static RGBW White Boost", test_static_white_boost},
        {"scalar routing and reverse direction", test_scalar_sources_and_reverse_direction},
        {"independent state and effect reset", test_state_isolation_and_effect_reset},
        {"state reset compatibility", test_state_reset_compatibility},
        {"spectrum and macro routing", test_spectrum_and_macro_routing},
        {"stereo, mirrored, macro, and response geometry",
         test_stereo_mirrored_macro_and_response_geometry},
        {"VU colour modes and centre geometry", test_vu_colour_modes_and_centre_geometry},
        {"catalog dynamic effects and independent state",
         test_catalog_dynamic_effects_and_independent_state},
        {"running rainbow per-pixel spacing",
         test_running_rainbow_uses_per_pixel_spacing},
        {"segmented smoothing once per frame and convergence",
         test_segment_smoothing_is_once_per_frame_and_converges},
        {"all effects short-span bounds",
         test_all_effects_handle_short_spans_without_out_of_bounds_writes},
        {"reset default and diagnostic scene data",
         test_reset_default_and_diagnostic_scene_data},
    }};

    for (const NamedTest& test : kTests) {
        if (!test.function()) {
            std::fprintf(stderr, "effect engine test failed: %s\n", test.name);
            return 1;
        }
    }

    return 0;
}
