#include "config/runtime_publication.hpp"
namespace config {
PublicationStatus
RuntimePublicationCoordinator::publish(const DeviceConfiguration &previous,
                                       const DeviceConfiguration &next) {
  if (!validate(next))
    return PublicationStatus::validation_failed;
  if (!backend_.prepare(next))
    return PublicationStatus::prepare_failed;
  if (!backend_.acquire_new_resources(next))
    return PublicationStatus::claim_failed;
  if (!backend_.switch_led(next))
    return PublicationStatus::led_switch_failed;
  if (backend_.switch_effects_idle(next))
    return PublicationStatus::ok;
  if (backend_.rollback(previous))
    return PublicationStatus::effect_switch_failed;
  backend_.safe_disable();
  return PublicationStatus::rollback_failed_safe_disabled;
}
} // namespace config
