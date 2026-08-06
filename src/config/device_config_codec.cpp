#include "config/device_config_codec.hpp"
#include <algorithm>
namespace config {
namespace {
void p8(uint8_t *&p, uint8_t v) { *p++ = v; }
void p16(uint8_t *&p, uint16_t v) {
  p8(p, v & 255);
  p8(p, v >> 8);
}
uint8_t g8(const uint8_t *&p) { return *p++; }
uint16_t g16(const uint8_t *&p) {
  uint16_t v = p[0] | uint16_t(p[1]) << 8;
  p += 2;
  return v;
}
void pc(uint8_t *&p, RgbwColor c) {
  p8(p, c.red);
  p8(p, c.green);
  p8(p, c.blue);
  p8(p, c.white);
}
RgbwColor gc(const uint8_t *&p) {
  RgbwColor c{p[0], p[1], p[2], p[3]};
  p += 4;
  return c;
}
bool gb(const uint8_t *&p, bool &v) {
  auto x = g8(p);
  if (x > 1)
    return false;
  v = x;
  return true;
}
void pe(uint8_t *&p, const EffectDeviceConfig &v) {
  p8(p, v.enabled);
  p8(p, (uint8_t)v.type);
  p8(p, (uint8_t)v.source);
  pc(p, v.primary_color);
  pc(p, v.secondary_color);
  pc(p, v.background_color);
  for (auto c : v.palette)
    pc(p, c);
  p8(p, (uint8_t)v.static_color_mode);
  p16(p, v.white_drive_percent);
  pc(p, v.rgb_assist_color);
  for (auto c : v.gyver_frequency_colors)
    pc(p, c);
  p8(p, (uint8_t)v.vu_color_mode);
  p8(p, (uint8_t)v.frequency_selection);
  p8(p, (uint8_t)v.gyver_full_strip_selection);
  p8(p, (uint8_t)v.gyver_running_frequencies_selection);
  p8(p, (uint8_t)v.macro_band_mapping);
  p16(p, v.animation_speed_q8);
  p16(p, v.color_spacing_q8);
  p16(p, v.fade_decay_ms);
  p8(p, v.strobe_frequency_hz);
  p8(p, v.strobe_duty_percent);
  p16(p, v.strobe_fade_ms);
  p8(p, (uint8_t)v.strobe_envelope_mode);
  p16(p, v.background_brightness_q8);
  p8(p, v.auto_gain_enabled);
  p16(p, v.auto_gain_headroom_q8);
  p16(p, v.adaptive_fast_response_ms);
  p16(p, v.adaptive_average_response_ms);
  p16(p, v.adaptive_trigger_percent);
  p16(p, v.adaptive_event_decay_ms);
  p16(p, v.gyver_animation_interval_ms);
  p16(p, v.gyver_rainbow_span_percent);
  p16(p, v.auto_gain_reference_rise_ms);
  p16(p, v.auto_gain_reference_fall_ms);
  p8(p, v.frequency_comet_tail_percent);
  p16(p, v.frequency_comet_quiet_threshold);
  p8(p, v.reversed);
  p16(p, v.visual_gain);
  p16(p, v.attack_ms);
  p16(p, v.release_ms);
  p8(p, v.segment_count);
  p8(p, v.zone_count);
  p8(p, v.macro_region_count);
  for (int i = 0; i < 6; i++)
    p8(p, 0);
}
bool ge(const uint8_t *&p, EffectDeviceConfig &v) {
  if (!gb(p, v.enabled))
    return false;
  auto t = g8(p), s = g8(p);
  if (t > 22 || s > 11)
    return false;
  v.type = (effects::EffectType)t;
  v.source = (effects::EffectSource)s;
  v.primary_color = gc(p);
  v.secondary_color = gc(p);
  v.background_color = gc(p);
  for (auto &c : v.palette)
    c = gc(p);
  auto sm = g8(p);
  if (sm > 1)
    return false;
  v.static_color_mode = (effects::StaticColorMode)sm;
  v.white_drive_percent = g16(p);
  v.rgb_assist_color = gc(p);
  for (auto &c : v.gyver_frequency_colors)
    c = gc(p);
  auto vu = g8(p), fs = g8(p), a = g8(p), b = g8(p), m = g8(p);
  if (vu > 2 || fs > 3 || a > 1 || b > 1 || m > 1)
    return false;
  v.vu_color_mode = (effects::VuColorMode)vu;
  v.frequency_selection = (effects::FrequencySelection)fs;
  v.gyver_full_strip_selection = (effects::GyverFullStripSelectionPolicy)a;
  v.gyver_running_frequencies_selection =
      (effects::GyverFullStripSelectionPolicy)b;
  v.macro_band_mapping = (effects::GenericMacroBandMapping)m;
  v.animation_speed_q8 = g16(p);
  v.color_spacing_q8 = g16(p);
  v.fade_decay_ms = g16(p);
  v.strobe_frequency_hz = g8(p);
  v.strobe_duty_percent = g8(p);
  v.strobe_fade_ms = g16(p);
  auto se = g8(p);
  if (se > 1)
    return false;
  v.strobe_envelope_mode = (effects::StrobeEnvelopeMode)se;
  v.background_brightness_q8 = g16(p);
  if (!gb(p, v.auto_gain_enabled))
    return false;
  v.auto_gain_headroom_q8 = g16(p);
  v.adaptive_fast_response_ms = g16(p);
  v.adaptive_average_response_ms = g16(p);
  v.adaptive_trigger_percent = g16(p);
  v.adaptive_event_decay_ms = g16(p);
  v.gyver_animation_interval_ms = g16(p);
  v.gyver_rainbow_span_percent = g16(p);
  v.auto_gain_reference_rise_ms = g16(p);
  v.auto_gain_reference_fall_ms = g16(p);
  v.frequency_comet_tail_percent = g8(p);
  v.frequency_comet_quiet_threshold = g16(p);
  if (!gb(p, v.reversed))
    return false;
  v.visual_gain = g16(p);
  v.attack_ms = g16(p);
  v.release_ms = g16(p);
  v.segment_count = g8(p);
  v.zone_count = g8(p);
  v.macro_region_count = g8(p);
  for (int i = 0; i < 6; i++)
    if (g8(p) != 0)
      return false;
  return true;
}
} // namespace
CodecStatus encode(const DeviceConfiguration &v, PayloadBuffer &o) {
  if (!validate(v))
    return CodecStatus::validation_failed;
  o.fill(0);
  uint8_t *p = o.data();
  p16(p, 0);
  p16(p, 0);
  for (const auto &c : v.led_channels) {
    p8(p, c.enabled);
    p16(p, c.pixel_count);
    p8(p, (uint8_t)c.channel_order);
    p8(p, c.reversed);
    p8(p, c.brightness);
    p16(p, c.physical.length_mm);
    p16(p, c.physical.density_pixels_per_metre);
  }
  for (const auto &e : v.effects)
    pe(p, e);
  const auto &i = v.idle_lighting;
  p8(p, i.enabled);
  p8(p, i.startup_idle_enabled);
  p16(p, i.silence_timeout_ms);
  p16(p, i.audio_confirmation_ms);
  pc(p, i.idle_color_rgbw);
  p16(p, i.idle_brightness_q8);
  p16(p, i.fade_to_effect_ms);
  p16(p, i.fade_to_idle_ms);
  p8(p, i.activity_input_mask);
  p8(p, i.logical_channel_mask);
  p16(p, i.left_activity_floor);
  p16(p, i.right_activity_floor);
  p16(p, i.aux_activity_floor);
  p16(p, i.activity_hysteresis);
  for (int x = 0; x < 4; x++)
    p8(p, 0);
  const auto &a = v.audio_calibration;
  p16(p, a.gyver_left_noise_floor);
  p16(p, a.gyver_right_noise_floor);
  p16(p, a.gyver_noise_gate_hysteresis);
  p16(p, a.gyver_spectrum_noise_floor);
  p16(p, a.gyver_spectrum_minimum_peak);
  for (int x = 0; x < 6; x++)
    p8(p, 0);
  return p == o.data() + o.size() ? CodecStatus::ok : CodecStatus::malformed;
}
CodecStatus decode(const uint8_t *d, std::size_t n, DeviceConfiguration &o) {
  if (n != kSchema1PayloadSize)
    return CodecStatus::wrong_length;
  const uint8_t *p = d;
  if (g16(p) || g16(p))
    return CodecStatus::reserved_nonzero;
  DeviceConfiguration v{};
  for (auto &c : v.led_channels) {
    if (!gb(p, c.enabled))
      return CodecStatus::malformed;
    c.pixel_count = g16(p);
    auto x = g8(p);
    if (x > 1)
      return CodecStatus::malformed;
    c.channel_order = (ChannelOrder)x;
    if (!gb(p, c.reversed))
      return CodecStatus::malformed;
    c.brightness = g8(p);
    c.physical.length_mm = g16(p);
    c.physical.density_pixels_per_metre = g16(p);
  }
  for (auto &e : v.effects)
    if (!ge(p, e))
      return CodecStatus::malformed;
  auto &i = v.idle_lighting;
  if (!gb(p, i.enabled) || !gb(p, i.startup_idle_enabled))
    return CodecStatus::malformed;
  i.silence_timeout_ms = g16(p);
  i.audio_confirmation_ms = g16(p);
  i.idle_color_rgbw = gc(p);
  i.idle_brightness_q8 = g16(p);
  i.fade_to_effect_ms = g16(p);
  i.fade_to_idle_ms = g16(p);
  i.activity_input_mask = g8(p);
  i.logical_channel_mask = g8(p);
  i.left_activity_floor = g16(p);
  i.right_activity_floor = g16(p);
  i.aux_activity_floor = g16(p);
  i.activity_hysteresis = g16(p);
  for (int x = 0; x < 4; x++)
    if (g8(p))
      return CodecStatus::reserved_nonzero;
  auto &a = v.audio_calibration;
  a.gyver_left_noise_floor = g16(p);
  a.gyver_right_noise_floor = g16(p);
  a.gyver_noise_gate_hysteresis = g16(p);
  a.gyver_spectrum_noise_floor = g16(p);
  a.gyver_spectrum_minimum_peak = g16(p);
  for (int x = 0; x < 6; x++)
    if (g8(p))
      return CodecStatus::reserved_nonzero;
  if (p != d + n)
    return CodecStatus::malformed;
  if (!validate(v))
    return CodecStatus::validation_failed;
  o = v;
  return CodecStatus::ok;
}
} // namespace config
