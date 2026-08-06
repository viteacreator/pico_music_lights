#include "config/schema_enum_mapping.hpp"
namespace config::schema {
#define MAP2(name, type, a, b)                                                 \
  bool name##_to_id(type v, uint8_t &o) {                                      \
    switch (v) {                                                               \
    case type::a:                                                              \
      o = 0;                                                                   \
      return true;                                                             \
    case type::b:                                                              \
      o = 1;                                                                   \
      return true;                                                             \
    }                                                                          \
    return false;                                                              \
  }                                                                            \
  bool name##_from_id(uint8_t v, type &o) {                                    \
    switch (v) {                                                               \
    case 0:                                                                    \
      o = type::a;                                                             \
      return true;                                                             \
    case 1:                                                                    \
      o = type::b;                                                             \
      return true;                                                             \
    default:                                                                   \
      return false;                                                            \
    }                                                                          \
  }
MAP2(static_color, effects::StaticColorMode, direct_rgbw, white_boost)
MAP2(selection, effects::GyverFullStripSelectionPolicy, gyver_priority,
     strongest_event)
MAP2(macro, effects::GenericMacroBandMapping, low_mid_high, bass_mid_high)
MAP2(envelope, effects::StrobeEnvelopeMode, hard_cut, fade_envelope)
#undef MAP2
bool vu_color_to_id(effects::VuColorMode v, uint8_t &o) {
  switch (v) {
  case effects::VuColorMode::solid:
    o = 0;
    return true;
  case effects::VuColorMode::level_position_gradient:
    o = 1;
    return true;
  case effects::VuColorMode::animated_rainbow:
    o = 2;
    return true;
  }
  return false;
}
bool vu_color_from_id(uint8_t v, effects::VuColorMode &o) {
  switch (v) {
  case 0:
    o = effects::VuColorMode::solid;
    return true;
  case 1:
    o = effects::VuColorMode::level_position_gradient;
    return true;
  case 2:
    o = effects::VuColorMode::animated_rainbow;
    return true;
  default:
    return false;
  }
}
bool frequency_to_id(effects::FrequencySelection v, uint8_t &o) {
  switch (v) {
  case effects::FrequencySelection::three_frequencies:
    o = 0;
    return true;
  case effects::FrequencySelection::low:
    o = 1;
    return true;
  case effects::FrequencySelection::mid:
    o = 2;
    return true;
  case effects::FrequencySelection::high:
    o = 3;
    return true;
  }
  return false;
}
bool frequency_from_id(uint8_t v, effects::FrequencySelection &o) {
  switch (v) {
  case 0:
    o = effects::FrequencySelection::three_frequencies;
    return true;
  case 1:
    o = effects::FrequencySelection::low;
    return true;
  case 2:
    o = effects::FrequencySelection::mid;
    return true;
  case 3:
    o = effects::FrequencySelection::high;
    return true;
  default:
    return false;
  }
}
bool effect_type_to_id(effects::EffectType v, uint8_t &o) {
  switch (v) {
  case effects::EffectType::off:
    o = 0;
    return true;
  case effects::EffectType::static_rgbw:
    o = 1;
    return true;
  case effects::EffectType::scalar_vu:
    o = 2;
    return true;
  case effects::EffectType::stereo_center_out_vu:
    o = 3;
    return true;
  case effects::EffectType::spectrum_bars:
    o = 4;
    return true;
  case effects::EffectType::mirrored_spectrum_zones:
    o = 5;
    return true;
  case effects::EffectType::macro_bands:
    o = 6;
    return true;
  case effects::EffectType::one_band_frequency:
    o = 7;
    return true;
  case effects::EffectType::stroboscope:
    o = 8;
    return true;
  case effects::EffectType::ambient_color_cycle:
    o = 9;
    return true;
  case effects::EffectType::running_rainbow:
    o = 10;
    return true;
  case effects::EffectType::frequency_comet:
    o = 11;
    return true;
  case effects::EffectType::gyver_vu_gradient:
    o = 12;
    return true;
  case effects::EffectType::gyver_vu_rainbow:
    o = 13;
    return true;
  case effects::EffectType::gyver_frequency_5_zones:
    o = 14;
    return true;
  case effects::EffectType::gyver_frequency_3_zones:
    o = 15;
    return true;
  case effects::EffectType::gyver_frequency_full_strip:
    o = 16;
    return true;
  case effects::EffectType::gyver_stroboscope:
    o = 17;
    return true;
  case effects::EffectType::gyver_ambient_static:
    o = 18;
    return true;
  case effects::EffectType::gyver_ambient_color_cycle:
    o = 19;
    return true;
  case effects::EffectType::gyver_ambient_running_rainbow:
    o = 20;
    return true;
  case effects::EffectType::gyver_running_frequencies:
    o = 21;
    return true;
  case effects::EffectType::gyver_spectrum_analyzer:
    o = 22;
    return true;
  }
  return false;
}
bool effect_type_from_id(uint8_t v, effects::EffectType &o) {
  switch (v) {
  case 0:
    o = effects::EffectType::off;
    return true;
  case 1:
    o = effects::EffectType::static_rgbw;
    return true;
  case 2:
    o = effects::EffectType::scalar_vu;
    return true;
  case 3:
    o = effects::EffectType::stereo_center_out_vu;
    return true;
  case 4:
    o = effects::EffectType::spectrum_bars;
    return true;
  case 5:
    o = effects::EffectType::mirrored_spectrum_zones;
    return true;
  case 6:
    o = effects::EffectType::macro_bands;
    return true;
  case 7:
    o = effects::EffectType::one_band_frequency;
    return true;
  case 8:
    o = effects::EffectType::stroboscope;
    return true;
  case 9:
    o = effects::EffectType::ambient_color_cycle;
    return true;
  case 10:
    o = effects::EffectType::running_rainbow;
    return true;
  case 11:
    o = effects::EffectType::frequency_comet;
    return true;
  case 12:
    o = effects::EffectType::gyver_vu_gradient;
    return true;
  case 13:
    o = effects::EffectType::gyver_vu_rainbow;
    return true;
  case 14:
    o = effects::EffectType::gyver_frequency_5_zones;
    return true;
  case 15:
    o = effects::EffectType::gyver_frequency_3_zones;
    return true;
  case 16:
    o = effects::EffectType::gyver_frequency_full_strip;
    return true;
  case 17:
    o = effects::EffectType::gyver_stroboscope;
    return true;
  case 18:
    o = effects::EffectType::gyver_ambient_static;
    return true;
  case 19:
    o = effects::EffectType::gyver_ambient_color_cycle;
    return true;
  case 20:
    o = effects::EffectType::gyver_ambient_running_rainbow;
    return true;
  case 21:
    o = effects::EffectType::gyver_running_frequencies;
    return true;
  case 22:
    o = effects::EffectType::gyver_spectrum_analyzer;
    return true;
  default:
    return false;
  }
}
bool effect_source_to_id(effects::EffectSource v, uint8_t &o) {
  switch (v) {
  case effects::EffectSource::none:
    o = 0;
    return true;
  case effects::EffectSource::left:
    o = 1;
    return true;
  case effects::EffectSource::right:
    o = 2;
    return true;
  case effects::EffectSource::aux:
    o = 3;
    return true;
  case effects::EffectSource::mono:
    o = 4;
    return true;
  case effects::EffectSource::bass:
    o = 5;
    return true;
  case effects::EffectSource::low:
    o = 6;
    return true;
  case effects::EffectSource::mid:
    o = 7;
    return true;
  case effects::EffectSource::high:
    o = 8;
    return true;
  case effects::EffectSource::stereo_left_right:
    o = 9;
    return true;
  case effects::EffectSource::spectrum_32:
    o = 10;
    return true;
  case effects::EffectSource::macro_bands:
    o = 11;
    return true;
  }
  return false;
}
bool effect_source_from_id(uint8_t v, effects::EffectSource &o) {
  switch (v) {
  case 0:
    o = effects::EffectSource::none;
    return true;
  case 1:
    o = effects::EffectSource::left;
    return true;
  case 2:
    o = effects::EffectSource::right;
    return true;
  case 3:
    o = effects::EffectSource::aux;
    return true;
  case 4:
    o = effects::EffectSource::mono;
    return true;
  case 5:
    o = effects::EffectSource::bass;
    return true;
  case 6:
    o = effects::EffectSource::low;
    return true;
  case 7:
    o = effects::EffectSource::mid;
    return true;
  case 8:
    o = effects::EffectSource::high;
    return true;
  case 9:
    o = effects::EffectSource::stereo_left_right;
    return true;
  case 10:
    o = effects::EffectSource::spectrum_32;
    return true;
  case 11:
    o = effects::EffectSource::macro_bands;
    return true;
  default:
    return false;
  }
}
} // namespace config::schema
