#include "effects/effect_scenes.hpp"

namespace effects {

namespace {

constexpr uint16_t kDefaultVuAttackMs = 45u;
constexpr uint16_t kDefaultVuReleaseMs = 160u;
constexpr uint32_t kDiagnosticSceneDurationMs = 12000u;

StripEffectConfig make_canonical_stereo_vu() {
    StripEffectConfig config{};
    config.enabled = true;
    config.type = EffectType::stereo_center_out_vu;
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
    config.type = EffectType::ambient_color_cycle;
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

    if (scene_id == DiagnosticSceneId::vu_and_ambient) {
        StripEffectConfig scalar_vu{};
        scalar_vu.enabled = true;
        scalar_vu.type = EffectType::scalar_vu;
        scalar_vu.source = EffectSource::mono;
        scalar_vu.primary_color = {0, 255, 32, 0};
        scalar_vu.vu_color_mode = VuColorMode::solid;
        scalar_vu.attack_ms = kDefaultVuAttackMs;
        scalar_vu.release_ms = kDefaultVuReleaseMs;
        scene[0] = scalar_vu;

        StripEffectConfig rainbow_vu = make_canonical_stereo_vu();
        rainbow_vu.vu_color_mode = VuColorMode::animated_rainbow;
        rainbow_vu.animation_speed_q8 = 128u;
        rainbow_vu.color_spacing_q8 = 192u;
        scene[1] = rainbow_vu;

        StripEffectConfig static_white{};
        static_white.enabled = true;
        static_white.type = EffectType::static_rgbw;
        static_white.source = EffectSource::none;
        static_white.static_color_mode = StaticColorMode::direct_rgbw;
        static_white.primary_color = {16, 32, 64, 96};
        scene[2] = static_white;

        StripEffectConfig strobe{};
        strobe.enabled = true;
        strobe.type = EffectType::stroboscope;
        strobe.source = EffectSource::none;
        strobe.primary_color = {255, 255, 255, 0};
        strobe.strobe_frequency_hz = 8u;
        strobe.strobe_fade_ms = 40u;
        scene[3] = strobe;

        scene[4] = make_ambient_cycle();

        StripEffectConfig rainbow{};
        rainbow.enabled = true;
        rainbow.type = EffectType::running_rainbow;
        rainbow.source = EffectSource::none;
        rainbow.animation_speed_q8 = 96u;
        rainbow.color_spacing_q8 = 256u;
        scene[5] = rainbow;
        return scene;
    }

    StripEffectConfig spectrum{};
    spectrum.enabled = true;
    spectrum.type = EffectType::spectrum_bars;
    spectrum.source = EffectSource::spectrum_32;
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
    zones.type = EffectType::mirrored_spectrum_zones;
    zones.source = EffectSource::spectrum_32;
    zones.zone_count = 5u;
    zones.attack_ms = kDefaultVuAttackMs;
    zones.release_ms = kDefaultVuReleaseMs;
    zones.palette = spectrum.palette;
    scene[1] = zones;

    StripEffectConfig macro{};
    macro.enabled = true;
    macro.type = EffectType::macro_bands;
    macro.source = EffectSource::macro_bands;
    macro.macro_region_count = 3u;
    macro.attack_ms = kDefaultVuAttackMs;
    macro.release_ms = kDefaultVuReleaseMs;
    macro.palette = spectrum.palette;
    scene[2] = macro;

    StripEffectConfig one_band{};
    one_band.enabled = true;
    one_band.type = EffectType::one_band_frequency;
    one_band.source = EffectSource::macro_bands;
    one_band.frequency_selection = FrequencySelection::three_frequencies;
    one_band.primary_color = {0, 128, 255, 0};
    one_band.attack_ms = kDefaultVuAttackMs;
    one_band.release_ms = kDefaultVuReleaseMs;
    scene[3] = one_band;

    StripEffectConfig running_frequency = one_band;
    running_frequency.type = EffectType::running_frequency;
    running_frequency.frequency_selection = FrequencySelection::mid;
    running_frequency.primary_color = {255, 32, 0, 0};
    running_frequency.animation_speed_q8 = 80u;
    running_frequency.fade_decay_ms = 180u;
    scene[4] = running_frequency;

    StripEffectConfig white_boost{};
    white_boost.enabled = true;
    white_boost.type = EffectType::static_rgbw;
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
    if (scene == DiagnosticSceneId::vu_and_ambient) {
        return DiagnosticSceneId::spectrum_and_motion;
    }

    return DiagnosticSceneId::vu_and_ambient;
}

}  // namespace effects
