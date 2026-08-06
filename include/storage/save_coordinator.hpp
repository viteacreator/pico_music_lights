#pragma once
#include "config/config_diagnostics.hpp"
#include "config/config_service.hpp"
#include "storage/device_config_store.hpp"
namespace storage {
enum class CoordinatorStatus : uint8_t {
  ok,
  validation_failed,
  led_timeout,
  audio_timeout,
  activation_failed,
  storage_failed,
  target_invalid,
  commit_state_unknown,
  confirmation_required
};
struct SafePointMetrics {
  uint32_t led_wait_us = 0u;
  uint32_t audio_wait_us = 0u;
  uint32_t flash_critical_us = 0u;
  uint32_t dropped_led_frames = 0u;
  uint32_t paused_audio_blocks = 0u;
};
class SafePointController {
public:
  virtual ~SafePointController() = default;
  virtual bool prepare_activation(const config::DeviceConfiguration &) = 0;
  virtual bool acquire_led(uint32_t) = 0;
  virtual bool acquire_audio(uint32_t) = 0;
  virtual bool activate_prepared() = 0;
  virtual void begin_flash_critical() = 0;
  virtual void end_flash_critical() = 0;
  virtual void restore() = 0;
  virtual SafePointMetrics metrics() const = 0;
};
constexpr uint32_t kLedSafePointDeadlineUs = 50000u;
constexpr uint32_t kAudioSafePointDeadlineUs = 50000u;
class SaveCoordinator {
public:
  SaveCoordinator(config::ConfigService &service, DeviceConfigStore &store,
                  SafePointController &points,
                  config::ConfigurationDiagnostics &diagnostics)
      : service_(service), store_(store), points_(points),
        diagnostics_(diagnostics) {}
  CoordinatorStatus preview();
  CoordinatorStatus save();
  CoordinatorStatus reload();
  CoordinatorStatus factory_reset(uint32_t confirmation);
  static constexpr uint32_t kFactoryResetConfirmation = 0x46525354u;

private:
  CoordinatorStatus activate_only(const config::DeviceConfiguration &);
  CoordinatorStatus persist(const config::DeviceConfiguration &, bool reset);
  CoordinatorStatus finish(CoordinatorStatus, bool reset);
  config::ConfigService &service_;
  DeviceConfigStore &store_;
  SafePointController &points_;
  config::ConfigurationDiagnostics &diagnostics_;
};
} // namespace storage
