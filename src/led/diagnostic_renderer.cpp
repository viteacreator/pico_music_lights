#include "led/diagnostic_renderer.hpp"

#include "board/led_board_config.hpp"
#include "config/runtime_adapter.hpp"
#include "config/runtime_publication.hpp"
#include "effects/effect_scenes.hpp"
#include "led/diagnostic_rendering.hpp"
#include "led/led_output_manager.hpp"
#include "pico/time.h"
#include <algorithm>
#include <array>

namespace {

constexpr uint64_t kRendererIntervalUs = 33334u;

static_assert(board::kStripCount == 6, "Expected six LED strips");
static_assert(board::kMaxConfiguredPixels == 800, "Unexpected LED pool size");

LedOutputManager g_manager;
effects::EffectEngine g_effect_engine;
effects::IdleLightingController g_idle_lighting;
DiagnosticRendererStats g_stats;
bool g_initialized = false;
bool g_enabled = false;
uint64_t g_last_update_us = 0;
uint64_t g_diagnostic_scene_started_us = 0;
effects::DiagnosticSceneId g_active_diagnostic_scene =
    effects::DiagnosticSceneId::gyver_vu_and_ambient;
bool g_diagnostic_scenes_enabled = false;
uint16_t g_volatile_gyver_left_noise_floor = 32u;
uint16_t g_volatile_gyver_right_noise_floor = 32u;
config::DeviceConfiguration g_active_configuration{};
constexpr uint16_t kVolatileGyverVuHysteresis = 4u;

bool is_gyver_vu(const effects::StripEffectConfig &config) {
  return config.type == effects::EffectType::gyver_vu_gradient ||
         config.type == effects::EffectType::gyver_vu_rainbow;
}

void apply_volatile_gyver_vu_noise_floors(
    std::array<effects::StripEffectConfig, effects::kEffectStripCount> &scene) {
  for (effects::StripEffectConfig &config : scene) {
    if (is_gyver_vu(config)) {
      config.gyver_left_noise_floor = g_volatile_gyver_left_noise_floor;
      config.gyver_right_noise_floor = g_volatile_gyver_right_noise_floor;
    }
  }
}

const char *scene_name(effects::DiagnosticSceneId scene) {
  switch (scene) {
  case effects::DiagnosticSceneId::gyver_vu_and_ambient:
    return "gyver_vu_ambient";

  case effects::DiagnosticSceneId::gyver_frequency_and_spectrum:
    return "gyver_frequency_spectrum";

  case effects::DiagnosticSceneId::extended_generic:
    return "extended_generic";

  case effects::DiagnosticSceneId::extended_ambient_and_frequency:
    return "extended_ambient_frequency";
  }

  return "unknown";
}

void stage_due_diagnostic_scene(uint64_t now_us) {
  if (!g_diagnostic_scenes_enabled) {
    return;
  }

  const uint64_t duration_us =
      static_cast<uint64_t>(
          effects::diagnostic_scene_duration_ms(g_active_diagnostic_scene)) *
      1000u;
  if (g_diagnostic_scene_started_us != 0u &&
      now_us - g_diagnostic_scene_started_us < duration_us) {
    return;
  }

  if (g_diagnostic_scene_started_us != 0u) {
    g_active_diagnostic_scene =
        effects::next_diagnostic_scene(g_active_diagnostic_scene);
  }

  std::array<effects::StripEffectConfig, effects::kEffectStripCount> scene =
      effects::diagnostic_scene(g_active_diagnostic_scene);
  apply_volatile_gyver_vu_noise_floors(scene);
  if (g_effect_engine.stage_scene(scene) == effects::EffectStatus::ok) {
    g_diagnostic_scene_started_us = now_us;
  } else {
    ++g_stats.effect_frames_failed;
  }
}

LogicalRgbwPixels pixels_for_strip(std::size_t strip_index) {
  LedStrip *const strip = g_manager.strip(strip_index);

  if (strip == nullptr) {
    return {};
  }

  return {strip->logical_pixels(), strip->pixel_count()};
}

bool build_runtime_configuration(
    const config::DeviceConfiguration &configuration,
    std::array<LedStripConfig, board::kStripCount> &led_configs,
    std::array<effects::StripEffectConfig, effects::kEffectStripCount> &effects,
    effects::IdleLightingConfig &idle) {
  config::CurrentBoardRuntimeConfiguration runtime{};
  if (!config::adapt_current_board(configuration, runtime)) {
    return false;
  }
  led_configs = runtime.led;
  effects = runtime.effects;
  idle = runtime.idle;
  return true;
}

bool configure_manager(const config::DeviceConfiguration &configuration) {
  std::array<LedStripConfig, board::kStripCount> led_configs{};
  std::array<effects::StripEffectConfig, effects::kEffectStripCount> effects{};
  effects::IdleLightingConfig idle{};
  if (!build_runtime_configuration(configuration, led_configs, effects, idle)) {
    return false;
  }
  if (g_manager.configure(led_configs) != LedStatus::ok ||
      g_effect_engine.stage_scene(effects) != effects::EffectStatus::ok ||
      g_idle_lighting.stage_config(idle) != effects::IdleLightingStatus::ok) {
    return false;
  }
  (void)g_effect_engine.apply_pending();
  (void)g_idle_lighting.apply_pending_config();
  g_volatile_gyver_left_noise_floor =
      configuration.audio_calibration.gyver_left_noise_floor;
  g_volatile_gyver_right_noise_floor =
      configuration.audio_calibration.gyver_right_noise_floor;
  const LedStatus initialization = g_manager.initialize_drivers();
  return initialization == LedStatus::ok ||
         initialization == LedStatus::partial_success;
}

class RendererPublicationBackend final
    : public config::RuntimePublicationBackend {
public:
  bool prepare(const config::DeviceConfiguration &next) override {
    return build_runtime_configuration(next, prepared_led_, prepared_effects_,
                                       prepared_idle_);
  }
  bool acquire_new_resources(const config::DeviceConfiguration &) override {
    return g_manager.ensure_drivers(prepared_led_) == LedStatus::ok;
  }
  bool switch_led(const config::DeviceConfiguration &) override {
    for (std::size_t index = 0; index < previous_led_.size(); ++index) {
      const LedStrip *strip = g_manager.strip(index);
      if (strip == nullptr)
        return false;
      previous_led_[index] = strip->config();
    }
    return g_manager.configure(prepared_led_) == LedStatus::ok;
  }
  bool switch_effects_idle(const config::DeviceConfiguration &next) override {
    if (g_effect_engine.stage_scene(prepared_effects_) !=
        effects::EffectStatus::ok)
      return false;
    if (g_idle_lighting.stage_config(prepared_idle_) !=
        effects::IdleLightingStatus::ok) {
      g_effect_engine.cancel_pending();
      return false;
    }
    (void)g_effect_engine.apply_pending();
    (void)g_idle_lighting.apply_pending_config();
    g_volatile_gyver_left_noise_floor =
        next.audio_calibration.gyver_left_noise_floor;
    g_volatile_gyver_right_noise_floor =
        next.audio_calibration.gyver_right_noise_floor;
    g_diagnostic_scenes_enabled = false;
    return true;
  }
  bool rollback(const config::DeviceConfiguration &) override {
    g_effect_engine.cancel_pending();
    g_idle_lighting.cancel_pending_config();
    return g_manager.configure(previous_led_) == LedStatus::ok;
  }
  void safe_disable() override { g_enabled = false; }

private:
  std::array<LedStripConfig, board::kStripCount> prepared_led_{};
  std::array<LedStripConfig, board::kStripCount> previous_led_{};
  std::array<effects::StripEffectConfig, effects::kEffectStripCount>
      prepared_effects_{};
  effects::IdleLightingConfig prepared_idle_{};
};
RendererPublicationBackend g_publication_backend;
config::RuntimePublicationCoordinator g_publication{g_publication_backend};

} // namespace

bool diagnostic_renderer_initialize(
    const config::DeviceConfiguration &configuration) {
  if (g_initialized) {
    return true;
  }

  g_initialized = configure_manager(configuration);
  if (g_initialized)
    g_active_configuration = configuration;
  return g_initialized;
}

bool diagnostic_renderer_wait_until_idle(uint32_t deadline_us) {
  const uint64_t deadline = time_us_64() + deadline_us;
  while (g_manager.is_frame_in_progress()) {
    diagnostic_renderer_service();
    if (time_us_64() >= deadline) {
      return false;
    }
  }
  return true;
}

bool diagnostic_renderer_publish_configuration(
    const config::DeviceConfiguration &configuration) {
  if (!g_initialized || g_manager.is_frame_in_progress()) {
    return false;
  }
  const config::PublicationStatus status =
      g_publication.publish(g_active_configuration, configuration);
  if (status != config::PublicationStatus::ok)
    return false;
  g_active_configuration = configuration;
  return true;
}

bool diagnostic_renderer_set_enabled(bool enabled) {
  if (!g_initialized) {
    return false;
  }

  g_enabled = enabled;
  return true;
}

bool diagnostic_renderer_is_enabled() { return g_enabled; }

std::size_t diagnostic_renderer_usable_strip_count() {
  return g_initialized ? g_manager.usable_strip_count() : 0u;
}

void diagnostic_renderer_service() {
  if (!g_initialized || !g_manager.is_frame_in_progress()) {
    return;
  }

  const LedStatus status = g_manager.poll_frame_completion();

  if (status == LedStatus::busy) {
    return;
  }

  g_stats.led_last_status = status;
  if (status == LedStatus::ok) {
    ++g_stats.led_frames_completed;
    ++g_stats.effect_frames_completed;
  } else if (status == LedStatus::transmission_timeout) {
    ++g_stats.led_frame_timeouts;
  }
}

void diagnostic_renderer_update(const AudioLevelFrame &audio,
                                const SpectrumFrame &spectrum) {
  diagnostic_renderer_service();

  if (!g_initialized) {
    return;
  }

  const uint64_t now_us = time_us_64();
  const bool renderer_update_due =
      g_enabled && now_us - g_last_update_us >= kRendererIntervalUs;

  if (!renderer_update_due) {
    return;
  }

  if (g_manager.is_frame_in_progress()) {
    // One due diagnostic frame could not start. Advance the schedule so
    // normal-loop polls during the same busy interval do not inflate this
    // counter.
    g_last_update_us = now_us;
    ++g_stats.led_frames_skipped_busy;
    ++g_stats.effect_frames_skipped_busy;
    return;
  }

  if (!diagnostic_frame_should_start(g_enabled, false, now_us, g_last_update_us,
                                     kRendererIntervalUs)) {
    return;
  }

  std::array<effects::EffectRenderSpan, board::kStripCount> spans{};
  for (std::size_t index = 0; index < spans.size(); ++index) {
    const LogicalRgbwPixels pixels = pixels_for_strip(index);
    spans[index] = {pixels.data, pixels.size};
  }

  const uint64_t render_start_us = time_us_64();
  stage_due_diagnostic_scene(render_start_us);
  const effects::EffectInputSnapshot snapshot{
      &audio,
      &spectrum,
      render_start_us,
  };
  g_effect_engine.render(snapshot, spans);
  (void)g_idle_lighting.apply_pending_config();
  g_idle_lighting.update(audio, render_start_us);
  g_idle_lighting.blend(spans);
  g_stats.effect_render_us =
      static_cast<uint32_t>(time_us_64() - render_start_us);
  g_stats.effect_render_max_us =
      std::max(g_stats.effect_render_max_us, g_stats.effect_render_us);

  const LedStatus status = g_manager.start_show_all_enabled();

  if (status == LedStatus::ok) {
    g_last_update_us = now_us;
    ++g_stats.led_frames_started;
    ++g_stats.effect_frames_started;
  } else if (status == LedStatus::busy) {
    g_last_update_us = now_us;
    ++g_stats.led_frames_skipped_busy;
    ++g_stats.effect_frames_skipped_busy;
  } else {
    g_stats.led_last_status = status;
    ++g_stats.effect_frames_failed;
  }
}

const DiagnosticRendererStats &diagnostic_renderer_stats() { return g_stats; }

void diagnostic_renderer_reset_stats() { g_stats = {}; }

bool diagnostic_renderer_read_effect_config(
    std::size_t strip_index, effects::StripEffectConfig &output) {
  return g_effect_engine.read_config(strip_index, output);
}

effects::EffectStatus diagnostic_renderer_stage_effect_config(
    std::size_t strip_index, const effects::StripEffectConfig &config) {
  const effects::EffectStatus status =
      g_effect_engine.stage_strip_config(strip_index, config);
  if (status == effects::EffectStatus::ok) {
    g_diagnostic_scenes_enabled = false;
  }
  return status;
}

bool diagnostic_renderer_set_volatile_gyver_vu_noise_floors(
    uint16_t left_floor, uint16_t right_floor) {
  g_volatile_gyver_left_noise_floor = left_floor;
  g_volatile_gyver_right_noise_floor = right_floor;

  std::array<effects::StripEffectConfig, effects::kEffectStripCount> scene{};
  for (std::size_t index = 0u; index < scene.size(); ++index) {
    if (!g_effect_engine.read_config(index, scene[index])) {
      return false;
    }
  }
  apply_volatile_gyver_vu_noise_floors(scene);
  return g_effect_engine.stage_scene(scene) == effects::EffectStatus::ok;
}

bool diagnostic_renderer_volatile_gyver_vu_noise_floors(uint16_t &left_floor,
                                                        uint16_t &right_floor) {
  left_floor = g_volatile_gyver_left_noise_floor;
  right_floor = g_volatile_gyver_right_noise_floor;
  return true;
}

uint16_t diagnostic_renderer_volatile_gyver_vu_hysteresis() {
  return kVolatileGyverVuHysteresis;
}

bool diagnostic_renderer_read_effect_runtime(
    std::size_t strip_index, DiagnosticEffectRuntimeSnapshot &output) {
  const effects::StripEffectRuntime *const runtime =
      g_effect_engine.runtime(strip_index);
  if (runtime == nullptr) {
    return false;
  }

  output.config = runtime->config;
  output.left_reference = runtime->state.auto_gain_references[0u];
  output.right_reference = runtime->state.auto_gain_references[1u];
  output.left_smoothed = runtime->state.smoothed_levels[0u];
  output.right_smoothed = runtime->state.smoothed_levels[1u];
  output.left_gate_open = runtime->state.gyver_left_noise_gate_open;
  output.right_gate_open = runtime->state.gyver_right_noise_gate_open;
  return true;
}

effects::EffectStatus diagnostic_renderer_stage_effect_scene(
    const std::array<effects::StripEffectConfig, effects::kEffectStripCount>
        &scene) {
  const effects::EffectStatus status = g_effect_engine.stage_scene(scene);
  if (status == effects::EffectStatus::ok) {
    g_diagnostic_scenes_enabled = false;
  }
  return status;
}

bool diagnostic_renderer_restore_default_effect_scene() {
  std::array<effects::StripEffectConfig, effects::kEffectStripCount> scene =
      effects::reset_default_scene();
  apply_volatile_gyver_vu_noise_floors(scene);
  if (g_effect_engine.stage_scene(scene) != effects::EffectStatus::ok) {
    return false;
  }
  g_diagnostic_scenes_enabled = false;
  return true;
}

uint32_t diagnostic_renderer_configuration_generation() {
  return g_effect_engine.configuration_generation();
}

bool diagnostic_renderer_read_idle_config(effects::IdleLightingConfig &output) {
  return g_idle_lighting.read_config(output);
}

effects::IdleLightingStatus diagnostic_renderer_stage_idle_config(
    const effects::IdleLightingConfig &config) {
  return g_idle_lighting.stage_config(config);
}

bool diagnostic_renderer_has_pending_idle_config() {
  return g_idle_lighting.has_pending_config();
}

bool diagnostic_renderer_apply_pending_idle_config() {
  return g_idle_lighting.apply_pending_config();
}

const effects::IdleLightingRuntime &diagnostic_renderer_idle_runtime() {
  return g_idle_lighting.runtime();
}

const char *diagnostic_renderer_active_scene_name() {
  return g_diagnostic_scenes_enabled ? scene_name(g_active_diagnostic_scene)
                                     : "custom_or_reset";
}
