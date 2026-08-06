#include "config/device_config.hpp"
#include "config/device_config_codec.hpp"
namespace config {
namespace {
constexpr uint16_t kMaximumAnimationSpeedQ8 = 4096u;
constexpr uint16_t kMaximumColorSpacingQ8 = 4096u;
constexpr uint16_t kMinimumAdaptiveTriggerPercent = 100u;
constexpr uint16_t kMaximumAdaptiveTriggerPercent = 1000u;
} // namespace
DeviceConfiguration make_factory_defaults() {
  DeviceConfiguration v{};
  constexpr uint16_t counts[6] = {132, 174, 141, 81, 96, 72};
  for (std::size_t i = 0; i < 6; ++i) {
    auto &l = v.led_channels[i];
    l.enabled = true;
    l.pixel_count = counts[i];
    l.physical.density_pixels_per_metre = board::kDefaultPixelsPerMetre;
    auto &e = v.effects[i];
    e.enabled = true;
    e.type = effects::EffectType::gyver_vu_gradient;
    e.source = effects::EffectSource::stereo_left_right;
    e.vu_color_mode = effects::VuColorMode::level_position_gradient;
    e.palette = {RgbwColor{0, 255, 0, 0}, RgbwColor{255, 255, 0, 0},
                 RgbwColor{255, 128, 0, 0}, RgbwColor{255, 0, 0, 0}};
    e.attack_ms = 45;
    e.release_ms = 160;
  }
  return v;
}
ValidationResult validate(const DeviceConfiguration &v) {
  uint32_t total = 0;
  for (std::size_t i = 0; i < v.led_channels.size(); ++i) {
    const auto &c = v.led_channels[i];
    if (static_cast<uint8_t>(c.channel_order) > 1)
      return {ValidationError::invalid_channel_order, (uint8_t)i,
              ValidationSection::led, ValidationField::channel_order,
              ValidationReason::unsupported};
    if (i >= board::kStripCount && c.enabled)
      return {ValidationError::unsupported_channel, (uint8_t)i,
              ValidationSection::led, ValidationField::enabled,
              ValidationReason::unsupported};
    if (c.enabled) {
      if (c.pixel_count < 1 || c.pixel_count > board::kMaxPixelsPerStrip)
        return {ValidationError::invalid_pixel_count, (uint8_t)i,
                ValidationSection::led, ValidationField::pixel_count,
                ValidationReason::out_of_range};
      total += c.pixel_count;
    }
  }
  if (total > board::kMaxConfiguredPixels)
    return {ValidationError::total_pixels, 0xff, ValidationSection::led,
            ValidationField::total_pixel_count, ValidationReason::out_of_range};
  const auto &idle = v.idle_lighting;
  if ((idle.logical_channel_mask & 0xc0u) != 0u)
    return {ValidationError::invalid_idle, 0xff, ValidationSection::idle,
            ValidationField::logical_channel_mask,
            ValidationReason::unsupported};
  if ((idle.activity_input_mask & ~effects::kIdleAllInputs) != 0u)
    return {ValidationError::invalid_idle, 0xff, ValidationSection::idle,
            ValidationField::activity_input_mask,
            ValidationReason::unsupported};
  if (idle.idle_brightness_q8 > effects::kEffectUnityGain)
    return {ValidationError::invalid_idle, 0xff, ValidationSection::idle,
            ValidationField::brightness, ValidationReason::out_of_range};
  if (idle.silence_timeout_ms > effects::kIdleMaximumTimingMs)
    return {ValidationError::invalid_idle, 0xff, ValidationSection::idle,
            ValidationField::silence_timeout_ms,
            ValidationReason::out_of_range};
  if (idle.audio_confirmation_ms > effects::kIdleMaximumTimingMs)
    return {ValidationError::invalid_idle, 0xff, ValidationSection::idle,
            ValidationField::audio_confirmation_ms,
            ValidationReason::out_of_range};
  if (idle.fade_to_effect_ms > effects::kIdleMaximumTimingMs)
    return {ValidationError::invalid_idle, 0xff, ValidationSection::idle,
            ValidationField::fade_to_effect_ms, ValidationReason::out_of_range};
  if (idle.fade_to_idle_ms > effects::kIdleMaximumTimingMs)
    return {ValidationError::invalid_idle, 0xff, ValidationSection::idle,
            ValidationField::fade_to_idle_ms, ValidationReason::out_of_range};
  if (idle.left_activity_floor > 2047u)
    return {ValidationError::invalid_idle, 0xff, ValidationSection::idle,
            ValidationField::left_activity_floor,
            ValidationReason::out_of_range};
  if (idle.right_activity_floor > 2047u)
    return {ValidationError::invalid_idle, 0xff, ValidationSection::idle,
            ValidationField::right_activity_floor,
            ValidationReason::out_of_range};
  if (idle.aux_activity_floor > 2047u)
    return {ValidationError::invalid_idle, 0xff, ValidationSection::idle,
            ValidationField::aux_activity_floor,
            ValidationReason::out_of_range};
  if (idle.activity_hysteresis > 2047u)
    return {ValidationError::invalid_idle, 0xff, ValidationSection::idle,
            ValidationField::hysteresis, ValidationReason::out_of_range};
  if (v.audio_calibration.gyver_left_noise_floor > 2047)
    return {ValidationError::invalid_audio, 0xff, ValidationSection::audio,
            ValidationField::gyver_left_noise_floor,
            ValidationReason::out_of_range};
  if (v.audio_calibration.gyver_right_noise_floor > 2047)
    return {ValidationError::invalid_audio, 0xff, ValidationSection::audio,
            ValidationField::gyver_right_noise_floor,
            ValidationReason::out_of_range};
  if (v.audio_calibration.gyver_noise_gate_hysteresis > 2047)
    return {ValidationError::invalid_audio, 0xff, ValidationSection::audio,
            ValidationField::gyver_noise_gate_hysteresis,
            ValidationReason::out_of_range};
  for (std::size_t i = 0; i < v.effects.size(); ++i) {
    if (v.effects[i].type == effects::EffectType::off && v.effects[i].enabled)
      return {ValidationError::invalid_effect, static_cast<uint8_t>(i),
              ValidationSection::effect, ValidationField::enabled,
              ValidationReason::incompatible};
    const auto runtime =
        to_runtime(canonicalize_effect(v.effects[i]), v.audio_calibration);
    const effects::EffectMetadata *metadata =
        effects::effect_metadata(runtime.type);
    const uint32_t mask = metadata == nullptr ? 0u : metadata->parameter_mask;
    if (metadata == nullptr)
      return {ValidationError::invalid_effect, static_cast<uint8_t>(i),
              ValidationSection::effect, ValidationField::effect_type,
              ValidationReason::unsupported};
    if (static_cast<uint8_t>(runtime.vu_color_mode) > 2u)
      return {ValidationError::invalid_effect, static_cast<uint8_t>(i),
              ValidationSection::effect, ValidationField::vu_color_mode,
              ValidationReason::unsupported};
    if (static_cast<uint8_t>(runtime.static_color_mode) > 1u)
      return {ValidationError::invalid_effect, static_cast<uint8_t>(i),
              ValidationSection::effect, ValidationField::static_color_mode,
              ValidationReason::unsupported};
    if (static_cast<uint8_t>(runtime.frequency_selection) > 3u)
      return {ValidationError::invalid_effect, static_cast<uint8_t>(i),
              ValidationSection::effect, ValidationField::frequency_selection,
              ValidationReason::unsupported};
    if (static_cast<uint8_t>(runtime.gyver_full_strip_selection) > 1u)
      return {ValidationError::invalid_effect, static_cast<uint8_t>(i),
              ValidationSection::effect, ValidationField::full_strip_selection,
              ValidationReason::unsupported};
    if (static_cast<uint8_t>(runtime.gyver_running_frequencies_selection) > 1u)
      return {ValidationError::invalid_effect, static_cast<uint8_t>(i),
              ValidationSection::effect, ValidationField::running_selection,
              ValidationReason::unsupported};
    if (static_cast<uint8_t>(runtime.macro_band_mapping) > 1u)
      return {ValidationError::invalid_effect, static_cast<uint8_t>(i),
              ValidationSection::effect, ValidationField::macro_band_mapping,
              ValidationReason::unsupported};
    if (static_cast<uint8_t>(runtime.strobe_envelope_mode) > 1u)
      return {ValidationError::invalid_effect, static_cast<uint8_t>(i),
              ValidationSection::effect, ValidationField::strobe_envelope_mode,
              ValidationReason::unsupported};
#define CHECK_EFFECT_FIELD(condition, field_name)                              \
  if (condition)                                                               \
  return {ValidationError::invalid_effect, static_cast<uint8_t>(i),            \
          ValidationSection::effect, ValidationField::field_name,              \
          ValidationReason::out_of_range}
    CHECK_EFFECT_FIELD(runtime.visual_gain > effects::kEffectMaximumGain,
                       visual_gain);
    CHECK_EFFECT_FIELD(runtime.attack_ms > effects::kEffectMaximumResponseMs,
                       attack_ms);
    CHECK_EFFECT_FIELD(runtime.release_ms > effects::kEffectMaximumResponseMs,
                       release_ms);
    CHECK_EFFECT_FIELD(runtime.fade_decay_ms >
                           effects::kEffectMaximumResponseMs,
                       fade_decay_ms);
    CHECK_EFFECT_FIELD(runtime.animation_speed_q8 > kMaximumAnimationSpeedQ8,
                       animation_speed_q8);
    CHECK_EFFECT_FIELD(runtime.color_spacing_q8 > kMaximumColorSpacingQ8,
                       color_spacing_q8);
    CHECK_EFFECT_FIELD(runtime.strobe_duty_percent > 100u, strobe_duty_percent);
    CHECK_EFFECT_FIELD(runtime.strobe_fade_ms >
                           effects::kEffectMaximumResponseMs,
                       strobe_fade_ms);
    CHECK_EFFECT_FIELD(runtime.background_brightness_q8 >
                           effects::kEffectMaximumGain,
                       background_brightness_q8);
    CHECK_EFFECT_FIELD(
        runtime.auto_gain_headroom_q8 < effects::kEffectUnityGain ||
            runtime.auto_gain_headroom_q8 > effects::kEffectMaximumGain,
        auto_gain_headroom_q8);
    CHECK_EFFECT_FIELD(runtime.adaptive_fast_response_ms >
                           effects::kEffectMaximumResponseMs,
                       adaptive_fast_response_ms);
    CHECK_EFFECT_FIELD(runtime.adaptive_average_response_ms >
                           effects::kEffectMaximumResponseMs,
                       adaptive_average_response_ms);
    CHECK_EFFECT_FIELD(
        runtime.adaptive_trigger_percent < kMinimumAdaptiveTriggerPercent ||
            runtime.adaptive_trigger_percent > kMaximumAdaptiveTriggerPercent,
        adaptive_trigger_percent);
    CHECK_EFFECT_FIELD(runtime.adaptive_event_decay_ms >
                           effects::kEffectMaximumResponseMs,
                       adaptive_event_decay_ms);
    CHECK_EFFECT_FIELD(runtime.gyver_animation_interval_ms >
                           effects::kEffectMaximumResponseMs,
                       gyver_animation_interval_ms);
    CHECK_EFFECT_FIELD(runtime.gyver_rainbow_span_percent > 400u,
                       gyver_rainbow_span_percent);
    CHECK_EFFECT_FIELD(runtime.auto_gain_reference_rise_ms >
                           effects::kEffectMaximumResponseMs,
                       auto_gain_reference_rise_ms);
    CHECK_EFFECT_FIELD(runtime.auto_gain_reference_fall_ms >
                           effects::kEffectMaximumResponseMs,
                       auto_gain_reference_fall_ms);
    CHECK_EFFECT_FIELD(runtime.frequency_comet_tail_percent > 100u,
                       frequency_comet_tail_percent);
    if (!effects::effect_supports_source(runtime.type, runtime.source))
      return {ValidationError::invalid_effect, static_cast<uint8_t>(i),
              ValidationSection::effect, ValidationField::source,
              ValidationReason::incompatible};
    if ((runtime.type == effects::EffectType::stroboscope ||
         runtime.type == effects::EffectType::gyver_stroboscope) &&
        (runtime.strobe_frequency_hz == 0u ||
         runtime.strobe_frequency_hz > 30u))
      return {ValidationError::invalid_effect, static_cast<uint8_t>(i),
              ValidationSection::effect, ValidationField::strobe_frequency_hz,
              ValidationReason::out_of_range};
    if (runtime.type == effects::EffectType::gyver_stroboscope &&
        runtime.strobe_envelope_mode != effects::StrobeEnvelopeMode::hard_cut)
      return {ValidationError::invalid_effect, static_cast<uint8_t>(i),
              ValidationSection::effect, ValidationField::strobe_envelope_mode,
              ValidationReason::incompatible};
    if (runtime.static_color_mode == effects::StaticColorMode::white_boost &&
        runtime.white_drive_percent > 200u)
      return {ValidationError::invalid_effect, static_cast<uint8_t>(i),
              ValidationSection::effect, ValidationField::white_drive_percent,
              ValidationReason::out_of_range};
    if (runtime.static_color_mode == effects::StaticColorMode::white_boost &&
        runtime.rgb_assist_color.white != 0u)
      return {ValidationError::invalid_effect, static_cast<uint8_t>(i),
              ValidationSection::effect, ValidationField::rgb_assist_color,
              ValidationReason::incompatible};
    if (runtime.type == effects::EffectType::spectrum_bars &&
        runtime.segment_count != 5u && runtime.segment_count != 8u &&
        runtime.segment_count != 16u && runtime.segment_count != 32u)
      return {ValidationError::invalid_effect, static_cast<uint8_t>(i),
              ValidationSection::effect, ValidationField::segment_count,
              ValidationReason::out_of_range};
    if (runtime.type == effects::EffectType::mirrored_spectrum_zones &&
        (runtime.zone_count == 0u ||
         runtime.zone_count > effects::kEffectMaximumMirroredZones))
      return {ValidationError::invalid_effect, static_cast<uint8_t>(i),
              ValidationSection::effect, ValidationField::zone_count,
              ValidationReason::out_of_range};
    if (runtime.type == effects::EffectType::macro_bands &&
        runtime.macro_region_count != 3u && runtime.macro_region_count != 4u)
      return {ValidationError::invalid_effect, static_cast<uint8_t>(i),
              ValidationSection::effect, ValidationField::macro_region_count,
              ValidationReason::out_of_range};
    const bool gyver_auto_gain =
        runtime.type == effects::EffectType::gyver_vu_gradient ||
        runtime.type == effects::EffectType::gyver_vu_rainbow ||
        runtime.type == effects::EffectType::gyver_spectrum_analyzer;
    if ((mask & effects::effect_parameter_gyver_adaptive) != 0u) {
      CHECK_EFFECT_FIELD(runtime.adaptive_fast_response_ms == 0u,
                         adaptive_fast_response_ms);
      CHECK_EFFECT_FIELD(runtime.adaptive_average_response_ms == 0u,
                         adaptive_average_response_ms);
      CHECK_EFFECT_FIELD(runtime.gyver_animation_interval_ms == 0u,
                         gyver_animation_interval_ms);
    }
    if (gyver_auto_gain) {
      CHECK_EFFECT_FIELD(runtime.auto_gain_reference_rise_ms == 0u,
                         auto_gain_reference_rise_ms);
      CHECK_EFFECT_FIELD(runtime.auto_gain_reference_fall_ms == 0u,
                         auto_gain_reference_fall_ms);
    }
#undef CHECK_EFFECT_FIELD
    const effects::EffectStatus status =
        effects::EffectEngine::validate_config(runtime);
    if (status != effects::EffectStatus::ok)
      return {ValidationError::invalid_effect, (uint8_t)i,
              ValidationSection::effect, ValidationField::effect_config,
              status == effects::EffectStatus::incompatible_source
                  ? ValidationReason::incompatible
                  : ValidationReason::out_of_range};
  }
  return {};
}
bool equal(const DeviceConfiguration &a, const DeviceConfiguration &b) {
  PayloadBuffer x{}, y{};
  return encode(a, x) == CodecStatus::ok && encode(b, y) == CodecStatus::ok &&
         x == y;
}
EffectDeviceConfig canonicalize_effect(const EffectDeviceConfig &value) {
  const effects::StripEffectConfig runtime_default =
      effects::canonical_effect_config(value.type);
  EffectDeviceConfig result{};
  result.type = value.type;
  result.enabled = value.type != effects::EffectType::off && value.enabled;
  result.source = runtime_default.source;
  result.primary_color = runtime_default.primary_color;
  result.secondary_color = runtime_default.secondary_color;
  result.background_color = runtime_default.background_color;
  result.palette = runtime_default.palette;
  result.static_color_mode = runtime_default.static_color_mode;
  result.white_drive_percent = runtime_default.white_drive_percent;
  result.rgb_assist_color = runtime_default.rgb_assist_color;
  result.gyver_frequency_colors = runtime_default.gyver_frequency_colors;
  result.vu_color_mode = runtime_default.vu_color_mode;
  result.frequency_selection = runtime_default.frequency_selection;
  result.gyver_full_strip_selection =
      runtime_default.gyver_full_strip_selection;
  result.gyver_running_frequencies_selection =
      runtime_default.gyver_running_frequencies_selection;
  result.macro_band_mapping = runtime_default.macro_band_mapping;
  result.animation_speed_q8 = runtime_default.animation_speed_q8;
  result.color_spacing_q8 = runtime_default.color_spacing_q8;
  result.fade_decay_ms = runtime_default.fade_decay_ms;
  result.strobe_frequency_hz = runtime_default.strobe_frequency_hz;
  result.strobe_duty_percent = runtime_default.strobe_duty_percent;
  result.strobe_fade_ms = runtime_default.strobe_fade_ms;
  result.strobe_envelope_mode = runtime_default.strobe_envelope_mode;
  result.background_brightness_q8 = runtime_default.background_brightness_q8;
  result.auto_gain_enabled = runtime_default.auto_gain_enabled;
  result.auto_gain_headroom_q8 = runtime_default.auto_gain_headroom_q8;
  result.adaptive_fast_response_ms = runtime_default.adaptive_fast_response_ms;
  result.adaptive_average_response_ms =
      runtime_default.adaptive_average_response_ms;
  result.adaptive_trigger_percent = runtime_default.adaptive_trigger_percent;
  result.adaptive_event_decay_ms = runtime_default.adaptive_event_decay_ms;
  result.gyver_animation_interval_ms =
      runtime_default.gyver_animation_interval_ms;
  result.gyver_rainbow_span_percent =
      runtime_default.gyver_rainbow_span_percent;
  result.auto_gain_reference_rise_ms =
      runtime_default.auto_gain_reference_rise_ms;
  result.auto_gain_reference_fall_ms =
      runtime_default.auto_gain_reference_fall_ms;
  result.frequency_comet_tail_percent =
      runtime_default.frequency_comet_tail_percent;
  result.frequency_comet_quiet_threshold =
      runtime_default.frequency_comet_quiet_threshold;
  result.reversed = runtime_default.reversed;
  result.visual_gain = runtime_default.visual_gain;
  result.attack_ms = runtime_default.attack_ms;
  result.release_ms = runtime_default.release_ms;
  result.segment_count = runtime_default.segment_count;
  result.zone_count = runtime_default.zone_count;
  result.macro_region_count = runtime_default.macro_region_count;

  const effects::EffectMetadata *metadata =
      effects::effect_metadata(value.type);
  const uint32_t mask = metadata == nullptr ? 0u : metadata->parameter_mask;
  if ((mask & effects::effect_parameter_source) != 0u)
    result.source = value.source;
  if ((mask & effects::effect_parameter_direction) != 0u)
    result.reversed = value.reversed;
  if ((mask & effects::effect_parameter_colours) != 0u) {
    result.primary_color = value.primary_color;
    result.secondary_color = value.secondary_color;
    result.background_color = value.background_color;
  }
  const bool gyver_frequency_colours =
      value.type == effects::EffectType::gyver_frequency_5_zones ||
      value.type == effects::EffectType::gyver_frequency_3_zones ||
      value.type == effects::EffectType::gyver_frequency_full_strip ||
      value.type == effects::EffectType::gyver_running_frequencies;
  if (gyver_frequency_colours)
    result.gyver_frequency_colors = value.gyver_frequency_colors;
  if ((mask & effects::effect_parameter_palette) != 0u)
    result.palette = value.palette;
  const bool vu_colour_mode =
      value.type == effects::EffectType::scalar_vu ||
      value.type == effects::EffectType::stereo_center_out_vu ||
      value.type == effects::EffectType::gyver_vu_gradient ||
      value.type == effects::EffectType::gyver_vu_rainbow;
  if (vu_colour_mode)
    result.vu_color_mode = value.vu_color_mode;
  if ((mask & effects::effect_parameter_gain) != 0u) {
    result.visual_gain = value.visual_gain;
    result.background_brightness_q8 = value.background_brightness_q8;
  }
  if ((mask & effects::effect_parameter_response) != 0u) {
    result.attack_ms = value.attack_ms;
    result.release_ms = value.release_ms;
    result.fade_decay_ms = value.fade_decay_ms;
  }
  if ((mask & effects::effect_parameter_animation) != 0u) {
    result.animation_speed_q8 = value.animation_speed_q8;
    result.color_spacing_q8 = value.color_spacing_q8;
    result.gyver_animation_interval_ms = value.gyver_animation_interval_ms;
    result.gyver_rainbow_span_percent = value.gyver_rainbow_span_percent;
  }
  if ((mask & effects::effect_parameter_geometry) != 0u) {
    result.segment_count = value.segment_count;
    result.zone_count = value.zone_count;
    result.macro_region_count = value.macro_region_count;
  }
  if ((mask & effects::effect_parameter_strobe) != 0u) {
    result.strobe_frequency_hz = value.strobe_frequency_hz;
    result.strobe_duty_percent = value.strobe_duty_percent;
    result.strobe_fade_ms = value.strobe_fade_ms;
    result.strobe_envelope_mode = value.strobe_envelope_mode;
  }
  if ((mask & effects::effect_parameter_gyver_adaptive) != 0u) {
    result.auto_gain_enabled = value.auto_gain_enabled;
    result.auto_gain_headroom_q8 = value.auto_gain_headroom_q8;
    result.adaptive_fast_response_ms = value.adaptive_fast_response_ms;
    result.adaptive_average_response_ms = value.adaptive_average_response_ms;
    result.adaptive_trigger_percent = value.adaptive_trigger_percent;
    result.adaptive_event_decay_ms = value.adaptive_event_decay_ms;
    result.auto_gain_reference_rise_ms = value.auto_gain_reference_rise_ms;
    result.auto_gain_reference_fall_ms = value.auto_gain_reference_fall_ms;
  }
  if ((mask & effects::effect_parameter_static_white_boost) != 0u) {
    result.static_color_mode = value.static_color_mode;
    result.white_drive_percent = value.white_drive_percent;
    result.rgb_assist_color = value.rgb_assist_color;
  }
  if ((mask & effects::effect_parameter_frequency_selection) != 0u)
    result.frequency_selection = value.frequency_selection;
  if ((mask & effects::effect_parameter_comet) != 0u) {
    result.frequency_comet_tail_percent = value.frequency_comet_tail_percent;
    result.frequency_comet_quiet_threshold =
        value.frequency_comet_quiet_threshold;
  }
  if ((mask & effects::effect_parameter_running_policy) != 0u)
    result.gyver_running_frequencies_selection =
        value.gyver_running_frequencies_selection;
  if ((mask & effects::effect_parameter_macro_mapping) != 0u)
    result.macro_band_mapping = value.macro_band_mapping;
  if ((mask & effects::effect_parameter_full_strip_policy) != 0u)
    result.gyver_full_strip_selection = value.gyver_full_strip_selection;
  return result;
}

effects::StripEffectConfig to_runtime(const EffectDeviceConfig &v,
                                      const AudioCalibrationConfig &c) {
  effects::StripEffectConfig r{};
  r.enabled = v.enabled;
  r.type = v.type;
  r.source = v.source;
  r.primary_color = v.primary_color;
  r.secondary_color = v.secondary_color;
  r.background_color = v.background_color;
  r.palette = v.palette;
  r.static_color_mode = v.static_color_mode;
  r.white_drive_percent = v.white_drive_percent;
  r.rgb_assist_color = v.rgb_assist_color;
  r.gyver_frequency_colors = v.gyver_frequency_colors;
  r.vu_color_mode = v.vu_color_mode;
  r.frequency_selection = v.frequency_selection;
  r.gyver_full_strip_selection = v.gyver_full_strip_selection;
  r.gyver_running_frequencies_selection = v.gyver_running_frequencies_selection;
  r.macro_band_mapping = v.macro_band_mapping;
  r.animation_speed_q8 = v.animation_speed_q8;
  r.color_spacing_q8 = v.color_spacing_q8;
  r.fade_decay_ms = v.fade_decay_ms;
  r.strobe_frequency_hz = v.strobe_frequency_hz;
  r.strobe_duty_percent = v.strobe_duty_percent;
  r.strobe_fade_ms = v.strobe_fade_ms;
  r.strobe_envelope_mode = v.strobe_envelope_mode;
  r.background_brightness_q8 = v.background_brightness_q8;
  r.auto_gain_enabled = v.auto_gain_enabled;
  r.auto_gain_headroom_q8 = v.auto_gain_headroom_q8;
  r.adaptive_fast_response_ms = v.adaptive_fast_response_ms;
  r.adaptive_average_response_ms = v.adaptive_average_response_ms;
  r.adaptive_trigger_percent = v.adaptive_trigger_percent;
  r.adaptive_event_decay_ms = v.adaptive_event_decay_ms;
  r.gyver_animation_interval_ms = v.gyver_animation_interval_ms;
  r.gyver_rainbow_span_percent = v.gyver_rainbow_span_percent;
  r.auto_gain_reference_rise_ms = v.auto_gain_reference_rise_ms;
  r.auto_gain_reference_fall_ms = v.auto_gain_reference_fall_ms;
  r.frequency_comet_tail_percent = v.frequency_comet_tail_percent;
  r.frequency_comet_quiet_threshold = v.frequency_comet_quiet_threshold;
  r.reversed = v.reversed;
  r.visual_gain = v.visual_gain;
  r.attack_ms = v.attack_ms;
  r.release_ms = v.release_ms;
  r.segment_count = v.segment_count;
  r.zone_count = v.zone_count;
  r.macro_region_count = v.macro_region_count;
  r.gyver_left_noise_floor = c.gyver_left_noise_floor;
  r.gyver_right_noise_floor = c.gyver_right_noise_floor;
  r.gyver_noise_gate_hysteresis = c.gyver_noise_gate_hysteresis;
  r.gyver_spectrum_noise_floor = c.gyver_spectrum_noise_floor;
  r.gyver_spectrum_minimum_peak = c.gyver_spectrum_minimum_peak;
  return r;
}
effects::IdleLightingConfig to_runtime(const IdleLightingDeviceConfig &v) {
  effects::IdleLightingConfig r{};
  r.enabled = v.enabled;
  r.startup_idle_enabled = v.startup_idle_enabled;
  r.silence_timeout_ms = v.silence_timeout_ms;
  r.audio_confirmation_ms = v.audio_confirmation_ms;
  r.idle_color_rgbw = v.idle_color_rgbw;
  r.idle_brightness_q8 = v.idle_brightness_q8;
  r.fade_to_effect_ms = v.fade_to_effect_ms;
  r.fade_to_idle_ms = v.fade_to_idle_ms;
  r.activity_input_mask = v.activity_input_mask;
  r.strip_enable_mask = v.logical_channel_mask & 0x3f;
  r.left_activity_floor = v.left_activity_floor;
  r.right_activity_floor = v.right_activity_floor;
  r.aux_activity_floor = v.aux_activity_floor;
  r.activity_hysteresis = v.activity_hysteresis;
  return r;
}
} // namespace config
