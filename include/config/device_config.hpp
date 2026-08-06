#pragma once
#include "board/led_board_config.hpp"
#include "effects/effect_engine.hpp"
#include "effects/idle_lighting.hpp"
#include "led/channel_order.hpp"
#include "led/rgbw_color.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace config {
constexpr std::size_t kMaximumLedChannelCount = 8;
constexpr std::size_t kCurrentBoardSupportedLedChannelCount =
    board::kStripCount;
constexpr uint16_t kPersistentPixelCountRepresentationMaximum = UINT16_MAX;
constexpr uint16_t kCurrentBoardMaximumPixelsPerChannel =
    board::kMaxPixelsPerStrip;
constexpr uint16_t kCurrentBoardMaximumTotalPixels =
    board::kMaxConfiguredPixels;
constexpr uint16_t kCurrentSchemaVersion = 1;
static_assert(kMaximumLedChannelCount == 8);
static_assert(kCurrentBoardSupportedLedChannelCount ==
              effects::kEffectStripCount);
static_assert(kCurrentBoardSupportedLedChannelCount < kMaximumLedChannelCount);

enum class ChannelOrder : uint8_t { rgbw = 0, grbw = 1 };
struct PhysicalStripMetadata {
  uint16_t length_mm = 0;
  uint16_t density_pixels_per_metre = 0;
};
struct LedChannelDeviceConfig {
  bool enabled = false;
  uint16_t pixel_count = 0;
  uint8_t brightness = 16;
  ChannelOrder channel_order = ChannelOrder::grbw;
  bool reversed = false;
  PhysicalStripMetadata physical{};
};
struct AudioCalibrationConfig {
  uint16_t gyver_left_noise_floor = 32;
  uint16_t gyver_right_noise_floor = 32;
  uint16_t gyver_noise_gate_hysteresis = 4;
  uint16_t gyver_spectrum_noise_floor = 256;
  uint16_t gyver_spectrum_minimum_peak = 64;
};
struct EffectDeviceConfig {
  bool enabled = false;
  effects::EffectType type = effects::EffectType::off;
  effects::EffectSource source = effects::EffectSource::none;
  RgbwColor primary_color{};
  RgbwColor secondary_color{};
  RgbwColor background_color{};
  std::array<RgbwColor, 4> palette{};
  effects::StaticColorMode static_color_mode =
      effects::StaticColorMode::direct_rgbw;
  uint16_t white_drive_percent = 100;
  RgbwColor rgb_assist_color{255, 255, 255, 0};
  std::array<RgbwColor, 3> gyver_frequency_colors{RgbwColor{255, 0, 0, 0},
                                                  RgbwColor{0, 255, 0, 0},
                                                  RgbwColor{255, 255, 0, 0}};
  effects::VuColorMode vu_color_mode = effects::VuColorMode::solid;
  effects::FrequencySelection frequency_selection =
      effects::FrequencySelection::three_frequencies;
  effects::GyverFullStripSelectionPolicy gyver_full_strip_selection =
      effects::GyverFullStripSelectionPolicy::gyver_priority;
  effects::GyverFullStripSelectionPolicy gyver_running_frequencies_selection =
      effects::GyverFullStripSelectionPolicy::gyver_priority;
  effects::GenericMacroBandMapping macro_band_mapping =
      effects::GenericMacroBandMapping::low_mid_high;
  uint16_t animation_speed_q8 = 256, color_spacing_q8 = 256,
           fade_decay_ms = 180;
  uint8_t strobe_frequency_hz = 8, strobe_duty_percent = 50;
  uint16_t strobe_fade_ms = 40;
  effects::StrobeEnvelopeMode strobe_envelope_mode =
      effects::StrobeEnvelopeMode::hard_cut;
  uint16_t background_brightness_q8 = 0;
  bool auto_gain_enabled = true;
  uint16_t auto_gain_headroom_q8 = 461, adaptive_fast_response_ms = 40,
           adaptive_average_response_ms = 700, adaptive_trigger_percent = 125,
           adaptive_event_decay_ms = 180, gyver_animation_interval_ms = 33,
           gyver_rainbow_span_percent = 50, auto_gain_reference_rise_ms = 300,
           auto_gain_reference_fall_ms = 1800;
  uint8_t frequency_comet_tail_percent = 20;
  uint16_t frequency_comet_quiet_threshold = 256;
  bool reversed = false;
  uint16_t visual_gain = 256, attack_ms = 0, release_ms = 0;
  uint8_t segment_count = 16, zone_count = 5, macro_region_count = 4;
};
struct IdleLightingDeviceConfig {
  bool enabled = false, startup_idle_enabled = true;
  uint16_t silence_timeout_ms = 10000, audio_confirmation_ms = 150;
  RgbwColor idle_color_rgbw{0, 0, 0, 255};
  uint16_t idle_brightness_q8 = 256, fade_to_effect_ms = 750,
           fade_to_idle_ms = 1500;
  uint8_t activity_input_mask = effects::kIdleAllInputs,
          logical_channel_mask = 0x3f;
  uint16_t left_activity_floor = 32, right_activity_floor = 32,
           aux_activity_floor = 32, activity_hysteresis = 4;
};
struct DeviceConfiguration {
  std::array<LedChannelDeviceConfig, kMaximumLedChannelCount> led_channels{};
  std::array<EffectDeviceConfig, kMaximumLedChannelCount> effects{};
  IdleLightingDeviceConfig idle_lighting{};
  AudioCalibrationConfig audio_calibration{};
};
enum class ValidationError : uint8_t {
  ok,
  unsupported_channel,
  invalid_pixel_count,
  total_pixels,
  invalid_channel_order,
  invalid_effect,
  invalid_idle,
  invalid_audio
};
enum class ValidationSection : uint8_t { none, led, effect, idle, audio };
enum class ValidationReason : uint8_t {
  none,
  unsupported,
  out_of_range,
  incompatible,
  reserved_nonzero,
  malformed
};
enum class ValidationField : uint8_t {
  none,
  enabled,
  pixel_count,
  channel_order,
  total_pixel_count,
  effect_config,
  source,
  effect_type,
  vu_color_mode,
  static_color_mode,
  frequency_selection,
  full_strip_selection,
  running_selection,
  macro_band_mapping,
  strobe_envelope_mode,
  visual_gain,
  attack_ms,
  release_ms,
  fade_decay_ms,
  animation_speed_q8,
  color_spacing_q8,
  strobe_frequency_hz,
  strobe_duty_percent,
  strobe_fade_ms,
  background_brightness_q8,
  auto_gain_headroom_q8,
  adaptive_fast_response_ms,
  adaptive_average_response_ms,
  adaptive_trigger_percent,
  adaptive_event_decay_ms,
  gyver_animation_interval_ms,
  gyver_rainbow_span_percent,
  auto_gain_reference_rise_ms,
  auto_gain_reference_fall_ms,
  frequency_comet_tail_percent,
  white_drive_percent,
  rgb_assist_color,
  segment_count,
  zone_count,
  macro_region_count,
  logical_channel_mask,
  activity_input_mask,
  timing,
  silence_timeout_ms,
  audio_confirmation_ms,
  fade_to_effect_ms,
  fade_to_idle_ms,
  brightness,
  activity_floor,
  left_activity_floor,
  right_activity_floor,
  aux_activity_floor,
  gyver_left_noise_floor,
  gyver_right_noise_floor,
  gyver_noise_gate_hysteresis,
  noise_floor,
  hysteresis
};
struct ValidationResult {
  ValidationError error = ValidationError::ok;
  uint8_t channel = 0xff;
  ValidationSection section = ValidationSection::none;
  ValidationField field = ValidationField::none;
  ValidationReason reason = ValidationReason::none;
  constexpr explicit operator bool() const {
    return error == ValidationError::ok;
  }
};
DeviceConfiguration make_factory_defaults();
ValidationResult validate(const DeviceConfiguration &value);
bool equal(const DeviceConfiguration &a, const DeviceConfiguration &b);
EffectDeviceConfig canonicalize_effect(const EffectDeviceConfig &value);
effects::StripEffectConfig
to_runtime(const EffectDeviceConfig &value,
           const AudioCalibrationConfig &calibration);
effects::IdleLightingConfig to_runtime(const IdleLightingDeviceConfig &value);
} // namespace config
