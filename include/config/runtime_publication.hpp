#pragma once
#include "config/device_config.hpp"
namespace config {
enum class PublicationStatus : uint8_t {
  ok,
  validation_failed,
  prepare_failed,
  claim_failed,
  led_switch_failed,
  effect_switch_failed,
  rollback_failed_safe_disabled
};
class RuntimePublicationBackend {
public:
  virtual ~RuntimePublicationBackend() = default;
  virtual bool prepare(const DeviceConfiguration &) = 0;
  virtual bool acquire_new_resources(const DeviceConfiguration &) = 0;
  virtual bool switch_led(const DeviceConfiguration &) = 0;
  virtual bool switch_effects_idle(const DeviceConfiguration &) = 0;
  virtual bool rollback(const DeviceConfiguration &) = 0;
  virtual void safe_disable() = 0;
};
class RuntimePublicationCoordinator {
public:
  explicit RuntimePublicationCoordinator(RuntimePublicationBackend &b)
      : backend_(b) {}
  PublicationStatus publish(const DeviceConfiguration &previous,
                            const DeviceConfiguration &next);

private:
  RuntimePublicationBackend &backend_;
};
} // namespace config
