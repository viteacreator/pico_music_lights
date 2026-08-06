#include "config/device_config.hpp"
#include "config/device_config_codec.hpp"
namespace config {
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
              ValidationSection::led, ValidationField::channel_order};
    if (i >= board::kStripCount && c.enabled)
      return {ValidationError::unsupported_channel, (uint8_t)i,
              ValidationSection::led, ValidationField::enabled};
    if (c.enabled) {
      if (c.pixel_count < 1 || c.pixel_count > board::kMaxPixelsPerStrip)
        return {ValidationError::invalid_pixel_count, (uint8_t)i,
                ValidationSection::led, ValidationField::pixel_count};
      total += c.pixel_count;
    }
  }
  if (total > board::kMaxConfiguredPixels)
    return {ValidationError::total_pixels, 0xff, ValidationSection::led,
            ValidationField::total_pixel_count};
  const auto &idle = v.idle_lighting;
  if ((idle.logical_channel_mask & 0xc0u) != 0u ||
      (idle.activity_input_mask & ~effects::kIdleAllInputs) != 0u ||
      idle.idle_brightness_q8 > effects::kEffectUnityGain ||
      idle.silence_timeout_ms > effects::kIdleMaximumTimingMs ||
      idle.audio_confirmation_ms > effects::kIdleMaximumTimingMs ||
      idle.fade_to_effect_ms > effects::kIdleMaximumTimingMs ||
      idle.fade_to_idle_ms > effects::kIdleMaximumTimingMs ||
      idle.left_activity_floor > 2047u || idle.right_activity_floor > 2047u ||
      idle.aux_activity_floor > 2047u || idle.activity_hysteresis > 2047u)
    return {ValidationError::invalid_idle, 0xff, ValidationSection::idle,
            ValidationField::timing};
  if (v.audio_calibration.gyver_left_noise_floor > 2047 ||
      v.audio_calibration.gyver_right_noise_floor > 2047 ||
      v.audio_calibration.gyver_noise_gate_hysteresis > 2047)
    return {ValidationError::invalid_audio, 0xff, ValidationSection::audio,
            ValidationField::noise_floor};
  for (std::size_t i = 0; i < v.effects.size(); ++i) {
    const auto runtime = to_runtime(v.effects[i], v.audio_calibration);
    if (effects::EffectEngine::validate_config(runtime) !=
        effects::EffectStatus::ok)
      return {ValidationError::invalid_effect, (uint8_t)i,
              ValidationSection::effect, ValidationField::effect_config};
  }
  return {};
}
bool equal(const DeviceConfiguration &a, const DeviceConfiguration &b) {
  PayloadBuffer x{}, y{};
  return encode(a, x) == CodecStatus::ok && encode(b, y) == CodecStatus::ok &&
         x == y;
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
