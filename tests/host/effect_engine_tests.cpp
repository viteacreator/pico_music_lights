#include "effects/effect_engine.hpp"

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
using effects::StaticColorMode;
using effects::StripEffectConfig;

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

    constexpr std::array<EffectType, effects::kEffectStripCount> kTypes{{
        EffectType::spectrum_bars,
        EffectType::mirrored_spectrum_zones,
        EffectType::macro_bands,
        EffectType::stereo_center_out_vu,
        EffectType::scalar_vu,
        EffectType::scalar_vu,
    }};
    constexpr std::array<EffectSource, effects::kEffectStripCount> kSources{{
        EffectSource::spectrum_32,
        EffectSource::spectrum_32,
        EffectSource::macro_bands,
        EffectSource::stereo_left_right,
        EffectSource::mono,
        EffectSource::aux,
    }};

    for (std::size_t index = 0; index < effects::kEffectStripCount; ++index) {
        StripEffectConfig config{};
        if (!engine.read_config(index, config) || !config.enabled ||
            config.type != kTypes[index] || config.source != kSources[index]) {
            return false;
        }
    }

    StripEffectConfig ignored{};
    return !engine.read_config(effects::kEffectStripCount, ignored) &&
           engine.runtime(effects::kEffectStripCount) == nullptr;
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

    config = scalar_config(EffectSource::left, kRed);
    if (engine.stage_strip_config(effects::kEffectStripCount, config) !=
        EffectStatus::invalid_strip_index) {
        return false;
    }

    std::array<StripEffectConfig, effects::kEffectStripCount> scene = off_scene();
    scene[0] = scalar_config(EffectSource::left, kRed);
    if (engine.stage_scene(scene) != EffectStatus::ok ||
        !engine.has_pending_configuration()) {
        return false;
    }

    StripEffectConfig active{};
    if (!engine.read_config(0u, active) || active.type != EffectType::spectrum_bars) {
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
    std::array<EffectRenderSpan, effects::kEffectStripCount> spans = empty_spans();
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

struct NamedTest {
    const char* name;
    bool (*function)();
};

}  // namespace

int main() {
    constexpr std::array<NamedTest, 9> kTests{{
        {"default scene and configuration API", test_default_scene_and_configuration_api},
        {"validation and atomic scene staging", test_validation_and_atomic_scene_staging},
        {"off, static RGBW, and span bounds", test_off_static_and_span_bounds},
        {"static RGBW White Boost", test_static_white_boost},
        {"scalar routing and reverse direction", test_scalar_sources_and_reverse_direction},
        {"independent state and effect reset", test_state_isolation_and_effect_reset},
        {"state reset compatibility", test_state_reset_compatibility},
        {"spectrum and macro routing", test_spectrum_and_macro_routing},
        {"stereo, mirrored, macro, and response geometry",
         test_stereo_mirrored_macro_and_response_geometry},
    }};

    for (const NamedTest& test : kTests) {
        if (!test.function()) {
            std::fprintf(stderr, "effect engine test failed: %s\n", test.name);
            return 1;
        }
    }

    return 0;
}
