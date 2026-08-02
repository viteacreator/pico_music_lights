#include "effects/effect_scenes.hpp"

namespace effects {

namespace {

constexpr uint16_t kDefaultVuAttackMs = 45u;
constexpr uint16_t kDefaultVuReleaseMs = 160u;
constexpr uint32_t kDiagnosticSceneDurationMs = 12000u;

StripEffectConfig make_canonical_stereo_vu() {
    StripEffectConfig config{};
    config.enabled = true;
    config.type = EffectType::gyver_vu_gradient;
    config.source = EffectSource::stereo_left_right;
    config.background_color = {0, 0, 0, 0};
    config.vu_color_mode = VuColorMode::level_position_gradient;
    config.palette = {
        RgbwColor{0, 255, 0, 0},
        RgbwColor{255, 255, 0, 0},
        RgbwColor{255, 128, 0, 0},
        RgbwColor{255, 0, 0, 0},
    };
    config.attack_ms = kDefaultVuAttackMs;
    config.release_ms = kDefaultVuReleaseMs;
    return config;
}

StripEffectConfig make_ambient_cycle() {
    StripEffectConfig config{};
    config.enabled = true;
    config.type = EffectType::gyver_ambient_color_cycle;
    config.source = EffectSource::none;
    config.animation_speed_q8 = 96u;
    return config;
}

}  // namespace

std::array<StripEffectConfig, kEffectStripCount> reset_default_scene() {
    std::array<StripEffectConfig, kEffectStripCount> scene{};
    scene.fill(make_canonical_stereo_vu());
    return scene;
}

std::array<StripEffectConfig, kEffectStripCount> diagnostic_scene(
    DiagnosticSceneId scene_id) {
    std::array<StripEffectConfig, kEffectStripCount> scene{};

    if (scene_id == DiagnosticSceneId::gyver_vu_and_ambient) {
        StripEffectConfig scalar_vu{};
        scalar_vu.enabled = true;
        scalar_vu.type = EffectType::gyver_vu_gradient;
        scalar_vu.source = EffectSource::stereo_left_right;
        scalar_vu.vu_color_mode = VuColorMode::level_position_gradient;
        scalar_vu.attack_ms = kDefaultVuAttackMs;
        scalar_vu.release_ms = kDefaultVuReleaseMs;
        scene[0] = scalar_vu;

        StripEffectConfig rainbow_vu = make_canonical_stereo_vu();
        rainbow_vu.type = EffectType::gyver_vu_rainbow;
        rainbow_vu.vu_color_mode = VuColorMode::animated_rainbow;
        rainbow_vu.animation_speed_q8 = 128u;
        rainbow_vu.color_spacing_q8 = 192u;
        scene[1] = rainbow_vu;

        StripEffectConfig static_white{};
        static_white.enabled = true;
        static_white.type = EffectType::gyver_ambient_static;
        static_white.source = EffectSource::none;
        static_white.static_color_mode = StaticColorMode::direct_rgbw;
        static_white.primary_color = {16, 32, 64, 96};
        scene[2] = static_white;

        StripEffectConfig strobe{};
        strobe.enabled = true;
        strobe.type = EffectType::gyver_stroboscope;
        strobe.source = EffectSource::none;
        strobe.primary_color = {255, 255, 255, 0};
        strobe.strobe_frequency_hz = 8u;
        strobe.strobe_fade_ms = 40u;
        scene[3] = strobe;

        scene[4] = make_ambient_cycle();

        StripEffectConfig rainbow{};
        rainbow.enabled = true;
        rainbow.type = EffectType::gyver_ambient_running_rainbow;
        rainbow.source = EffectSource::none;
        rainbow.animation_speed_q8 = 96u;
        rainbow.color_spacing_q8 = 256u;
        scene[5] = rainbow;
        return scene;
    }

    if (scene_id == DiagnosticSceneId::extended_generic) {
        StripEffectConfig generic_vu{};
        generic_vu.enabled = true;
        generic_vu.type = EffectType::scalar_vu;
        generic_vu.source = EffectSource::mono;
        generic_vu.primary_color = {0, 255, 32, 0};
        scene[0] = generic_vu;

        StripEffectConfig generic_spectrum{};
        generic_spectrum.enabled = true;
        generic_spectrum.type = EffectType::spectrum_bars;
        generic_spectrum.source = EffectSource::spectrum_32;
        generic_spectrum.segment_count = 16u;
        generic_spectrum.palette = make_canonical_stereo_vu().palette;
        scene[1] = generic_spectrum;

        StripEffectConfig generic_zones = generic_spectrum;
        generic_zones.type = EffectType::mirrored_spectrum_zones;
        generic_zones.zone_count = 5u;
        scene[2] = generic_zones;

        StripEffectConfig generic_macro{};
        generic_macro.enabled = true;
        generic_macro.type = EffectType::macro_bands;
        generic_macro.source = EffectSource::macro_bands;
        generic_macro.macro_region_count = 3u;
        generic_macro.palette = make_canonical_stereo_vu().palette;
        scene[3] = generic_macro;

        StripEffectConfig comet{};
        comet.enabled = true;
        comet.type = EffectType::frequency_comet;
        comet.source = EffectSource::macro_bands;
        comet.frequency_selection = FrequencySelection::mid;
        comet.primary_color = {255, 32, 0, 0};
        scene[4] = comet;

        StripEffectConfig rainbow{};
        rainbow.enabled = true;
        rainbow.type = EffectType::running_rainbow;
        rainbow.source = EffectSource::none;
        scene[5] = rainbow;
        return scene;
    }

    if (scene_id == DiagnosticSceneId::extended_ambient_and_frequency) {
        StripEffectConfig static_direct{};
        static_direct.enabled = true;
        static_direct.type = EffectType::static_rgbw;
        static_direct.source = EffectSource::none;
        static_direct.primary_color = {16, 32, 64, 96};
        scene[0] = static_direct;

        StripEffectConfig strobe{};
        strobe.enabled = true;
        strobe.type = EffectType::stroboscope;
        strobe.source = EffectSource::none;
        strobe.primary_color = {255, 255, 255, 0};
        scene[1] = strobe;

        scene[2] = make_ambient_cycle();

        StripEffectConfig one_band{};
        one_band.enabled = true;
        one_band.type = EffectType::one_band_frequency;
        one_band.source = EffectSource::macro_bands;
        one_band.frequency_selection = FrequencySelection::low;
        one_band.primary_color = {0, 160, 255, 0};
        scene[3] = one_band;

        StripEffectConfig white_boost{};
        white_boost.enabled = true;
        white_boost.type = EffectType::static_rgbw;
        white_boost.source = EffectSource::none;
        white_boost.static_color_mode = StaticColorMode::white_boost;
        white_boost.white_drive_percent = 125u;
        scene[4] = white_boost;

        StripEffectConfig stereo{};
        stereo.enabled = true;
        stereo.type = EffectType::stereo_center_out_vu;
        stereo.source = EffectSource::stereo_left_right;
        stereo.vu_color_mode = VuColorMode::animated_rainbow;
        scene[5] = stereo;
        return scene;
    }

    StripEffectConfig spectrum{};
    spectrum.enabled = true;
    spectrum.type = EffectType::gyver_frequency_5_zones;
    spectrum.source = EffectSource::macro_bands;
    spectrum.segment_count = 16u;
    spectrum.attack_ms = kDefaultVuAttackMs;
    spectrum.release_ms = kDefaultVuReleaseMs;
    spectrum.palette = {
        RgbwColor{0, 255, 0, 0},
        RgbwColor{255, 255, 0, 0},
        RgbwColor{255, 128, 0, 0},
        RgbwColor{255, 0, 0, 0},
    };
    scene[0] = spectrum;

    StripEffectConfig zones{};
    zones.enabled = true;
    zones.type = EffectType::gyver_frequency_3_zones;
    zones.source = EffectSource::macro_bands;
    zones.attack_ms = kDefaultVuAttackMs;
    zones.release_ms = kDefaultVuReleaseMs;
    zones.palette = spectrum.palette;
    scene[1] = zones;

    StripEffectConfig macro{};
    macro.enabled = true;
    macro.type = EffectType::gyver_frequency_full_strip;
    macro.source = EffectSource::macro_bands;
    macro.macro_region_count = 3u;
    macro.attack_ms = kDefaultVuAttackMs;
    macro.release_ms = kDefaultVuReleaseMs;
    macro.palette = spectrum.palette;
    scene[2] = macro;

    StripEffectConfig one_band{};
    one_band.enabled = true;
    one_band.type = EffectType::gyver_running_frequencies;
    one_band.source = EffectSource::macro_bands;
    one_band.frequency_selection = FrequencySelection::three_frequencies;
    one_band.primary_color = {0, 128, 255, 0};
    one_band.attack_ms = kDefaultVuAttackMs;
    one_band.release_ms = kDefaultVuReleaseMs;
    scene[3] = one_band;

    StripEffectConfig gyver_spectrum = spectrum;
    gyver_spectrum.type = EffectType::gyver_spectrum_analyzer;
    gyver_spectrum.source = EffectSource::spectrum_32;
    gyver_spectrum.segment_count = 16u;
    scene[4] = gyver_spectrum;

    StripEffectConfig white_boost{};
    white_boost.enabled = true;
    white_boost.type = EffectType::gyver_ambient_static;
    white_boost.source = EffectSource::none;
    white_boost.static_color_mode = StaticColorMode::white_boost;
    white_boost.white_drive_percent = 100u;
    scene[5] = white_boost;
    return scene;
}

uint32_t diagnostic_scene_duration_ms(DiagnosticSceneId scene) {
    (void)scene;
    return kDiagnosticSceneDurationMs;
}

DiagnosticSceneId next_diagnostic_scene(DiagnosticSceneId scene) {
    if (scene == DiagnosticSceneId::gyver_vu_and_ambient) {
        return DiagnosticSceneId::gyver_frequency_and_spectrum;
    }

    if (scene == DiagnosticSceneId::gyver_frequency_and_spectrum) {
        return DiagnosticSceneId::extended_generic;
    }

    if (scene == DiagnosticSceneId::extended_generic) {
        return DiagnosticSceneId::extended_ambient_and_frequency;
    }

    return DiagnosticSceneId::gyver_vu_and_ambient;
}

}  // namespace effects
