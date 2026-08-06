#pragma once
#include "effects/effect_engine.hpp"
#include <cstdint>
namespace config::schema {
bool effect_type_to_id(effects::EffectType, uint8_t &);
bool effect_type_from_id(uint8_t, effects::EffectType &);
bool effect_source_to_id(effects::EffectSource, uint8_t &);
bool effect_source_from_id(uint8_t, effects::EffectSource &);
bool vu_color_to_id(effects::VuColorMode, uint8_t &);
bool vu_color_from_id(uint8_t, effects::VuColorMode &);
bool static_color_to_id(effects::StaticColorMode, uint8_t &);
bool static_color_from_id(uint8_t, effects::StaticColorMode &);
bool frequency_to_id(effects::FrequencySelection, uint8_t &);
bool frequency_from_id(uint8_t, effects::FrequencySelection &);
bool selection_to_id(effects::GyverFullStripSelectionPolicy, uint8_t &);
bool selection_from_id(uint8_t, effects::GyverFullStripSelectionPolicy &);
bool macro_to_id(effects::GenericMacroBandMapping, uint8_t &);
bool macro_from_id(uint8_t, effects::GenericMacroBandMapping &);
bool envelope_to_id(effects::StrobeEnvelopeMode, uint8_t &);
bool envelope_from_id(uint8_t, effects::StrobeEnvelopeMode &);
} // namespace config::schema
