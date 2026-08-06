#pragma once
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
  commit_state_unknown,
  confirmation_required
};
class SafePointController {
public:
  virtual ~SafePointController() = default;
  virtual bool acquire_led(uint32_t) = 0;
  virtual bool acquire_audio(uint32_t) = 0;
  virtual bool activate(const config::DeviceConfiguration &) = 0;
  virtual void restore() = 0;
};
constexpr uint32_t kLedSafePointDeadlineUs = 50000,
                   kAudioSafePointDeadlineUs = 50000;
class SaveCoordinator {
public:
  SaveCoordinator(config::ConfigService &s, DeviceConfigStore &d,
                  SafePointController &p)
      : service_(s), store_(d), points_(p) {}
  CoordinatorStatus save();
  CoordinatorStatus factory_reset(uint32_t confirmation);
  static constexpr uint32_t kFactoryResetConfirmation = 0x46525354u;

private:
  CoordinatorStatus persist(const config::DeviceConfiguration &, bool);
  config::ConfigService &service_;
  DeviceConfigStore &store_;
  SafePointController &points_;
};
} // namespace storage
