#include "storage/save_coordinator.hpp"
namespace storage {
namespace {
config::OperationStatus diagnostic_status(CoordinatorStatus status) {
  using C = CoordinatorStatus;
  using D = config::OperationStatus;
  switch (status) {
  case C::ok:
    return D::ok;
  case C::validation_failed:
    return D::validation_failed;
  case C::led_timeout:
    return D::led_timeout;
  case C::audio_timeout:
    return D::audio_timeout;
  case C::activation_failed:
    return D::activation_failed;
  case C::target_invalid:
    return D::target_invalid;
  case C::commit_state_unknown:
    return D::commit_state_unknown;
  case C::confirmation_required:
    return D::confirmation_required;
  case C::storage_failed:
    return D::storage_failed;
  }
  return D::storage_failed;
}
} // namespace
CoordinatorStatus
SaveCoordinator::activate_only(const config::DeviceConfiguration &value) {
  diagnostics_.save_phase = config::OperationPhase::validation;
  diagnostics_.validation = config::validate(value);
  if (!diagnostics_.validation)
    return CoordinatorStatus::validation_failed;
  if (!points_.prepare_activation(value))
    return CoordinatorStatus::activation_failed;
  diagnostics_.save_phase = config::OperationPhase::led_safe_point;
  if (!points_.acquire_led(kLedSafePointDeadlineUs)) {
    points_.restore();
    return CoordinatorStatus::led_timeout;
  }
  // Ordinary preview publishes at the LED/render boundary only. The audio
  // safe point is reserved for flash Save and reset transactions.
  diagnostics_.save_phase = config::OperationPhase::activation;
  if (!points_.activate_prepared()) {
    points_.restore();
    return CoordinatorStatus::activation_failed;
  }
  service_.activation_succeeded(value);
  points_.restore();
  return CoordinatorStatus::ok;
}
CoordinatorStatus SaveCoordinator::preview() {
  const auto s = activate_only(service_.draft());
  diagnostics_.save_status = diagnostic_status(s);
  return s;
}
CoordinatorStatus SaveCoordinator::reload() {
  const auto s = activate_only(service_.discard_target());
  diagnostics_.save_status = diagnostic_status(s);
  return s;
}
CoordinatorStatus SaveCoordinator::save() {
  return persist(service_.draft(), false);
}
CoordinatorStatus SaveCoordinator::factory_reset(uint32_t confirmation) {
  if (confirmation != kFactoryResetConfirmation) {
    diagnostics_.reset_status = config::OperationStatus::confirmation_required;
    return CoordinatorStatus::confirmation_required;
  }
  return persist(service_.factory_defaults(), true);
}
CoordinatorStatus
SaveCoordinator::persist(const config::DeviceConfiguration &value, bool reset) {
  diagnostics_.save_phase = config::OperationPhase::validation;
  diagnostics_.validation = config::validate(value);
  if (!diagnostics_.validation)
    return CoordinatorStatus::validation_failed;
  diagnostics_.save_phase = config::OperationPhase::serialization;
  const SaveResult prepared = store_.prepare_save(value);
  if (prepared.status != StoreStatus::ok)
    return CoordinatorStatus::storage_failed;
  diagnostics_.target_slot = prepared.target;
  if (!points_.prepare_activation(value))
    return CoordinatorStatus::activation_failed;
  diagnostics_.save_phase = config::OperationPhase::led_safe_point;
  if (!points_.acquire_led(kLedSafePointDeadlineUs)) {
    points_.restore();
    return CoordinatorStatus::led_timeout;
  }
  diagnostics_.save_phase = config::OperationPhase::audio_safe_point;
  if (!points_.acquire_audio(kAudioSafePointDeadlineUs)) {
    points_.restore();
    return CoordinatorStatus::audio_timeout;
  }
  diagnostics_.save_phase = config::OperationPhase::activation;
  if (!points_.activate_prepared()) {
    points_.restore();
    return CoordinatorStatus::activation_failed;
  }
  service_.activation_succeeded(value);
  diagnostics_.save_phase = config::OperationPhase::erase;
  const SaveResult result = store_.commit_prepared();
  points_.restore();
  diagnostics_.commit_attempted = result.commit_attempted;
  CoordinatorStatus status = CoordinatorStatus::storage_failed;
  if (result.status == StoreStatus::ok) {
    service_.committed_save_succeeded(value);
    status = CoordinatorStatus::ok;
  } else if (result.status == StoreStatus::commit_state_unknown) {
    diagnostics_.commit_state_unknown = true;
    status = CoordinatorStatus::commit_state_unknown;
  } else if (result.status == StoreStatus::target_invalid) {
    status = CoordinatorStatus::target_invalid;
  }
  diagnostics_.dirty = service_.dirty();
  diagnostics_.active_differs_from_last_verified =
      !config::equal(service_.active(), service_.last_verified_persisted());
  diagnostics_.save_phase = config::OperationPhase::complete;
  if (reset)
    diagnostics_.reset_status = diagnostic_status(status);
  else
    diagnostics_.save_status = diagnostic_status(status);
  return status;
}
} // namespace storage
