#pragma once

#include "effects/effect_engine.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace effects {

// The reset scene is stable configuration data for a future user-facing
// "Reset to default" action. It does not select the temporary bring-up scene.
std::array<StripEffectConfig, kEffectStripCount> reset_default_scene();

// These value-returned scenes are deliberately isolated from the engine and
// transport. The application may stage them atomically for physical bring-up
// and later disable or remove that sequence without changing effect code.
enum class DiagnosticSceneId : uint8_t {
    vu_and_ambient,
    spectrum_and_motion,
};

constexpr std::size_t kDiagnosticSceneCount = 2u;

std::array<StripEffectConfig, kEffectStripCount> diagnostic_scene(
    DiagnosticSceneId scene);
uint32_t diagnostic_scene_duration_ms(DiagnosticSceneId scene);
DiagnosticSceneId next_diagnostic_scene(DiagnosticSceneId scene);

}  // namespace effects
