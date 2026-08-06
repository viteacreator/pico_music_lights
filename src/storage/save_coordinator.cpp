#include "storage/save_coordinator.hpp"
namespace storage {
CoordinatorStatus SaveCoordinator::save() {
  return persist(service_.draft(), false);
}
CoordinatorStatus SaveCoordinator::factory_reset(uint32_t c) {
  if (c != kFactoryResetConfirmation)
    return CoordinatorStatus::confirmation_required;
  return persist(service_.factory_defaults(), true);
}
CoordinatorStatus SaveCoordinator::persist(const config::DeviceConfiguration &v,
                                           bool reset) {
  if (!config::validate(v))
    return CoordinatorStatus::validation_failed;
  if (!points_.acquire_led(kLedSafePointDeadlineUs)) {
    points_.restore();
    return CoordinatorStatus::led_timeout;
  }
  if (!points_.acquire_audio(kAudioSafePointDeadlineUs)) {
    points_.restore();
    return CoordinatorStatus::audio_timeout;
  }
  if (!points_.activate(v)) {
    points_.restore();
    return CoordinatorStatus::activation_failed;
  }
  auto r = store_.save(v);
  points_.restore();
  if (r.status == StoreStatus::commit_state_unknown)
    return CoordinatorStatus::commit_state_unknown;
  if (r.status != StoreStatus::ok)
    return CoordinatorStatus::storage_failed;
  if (reset) {
    service_.replace_draft(v);
    service_.preview();
  }
  service_.mark_saved();
  return CoordinatorStatus::ok;
}
} // namespace storage
