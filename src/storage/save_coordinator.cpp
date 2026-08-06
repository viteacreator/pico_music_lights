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
config::OperationPhase diagnostic_phase(StorePhase phase) {
  using S = StorePhase;
  using D = config::OperationPhase;
  switch (phase) {
  case S::inspection:
    return D::inspection;
  case S::erase:
    return D::erase;
  case S::data_program:
    return D::data_program;
  case S::uncommitted_verify:
    return D::uncommitted_verify;
  case S::commit_program:
    return D::commit_program;
  case S::committed_inspection:
    return D::committed_inspection;
  case S::none:
    return D::idle;
  }
  return D::idle;
}
} // namespace
CoordinatorStatus SaveCoordinator::finish(CoordinatorStatus status,
                                          bool reset) {
  const SafePointMetrics metrics = points_.metrics();
  diagnostics_.led_safe_wait_us = metrics.led_wait_us;
  diagnostics_.audio_safe_wait_us = metrics.audio_wait_us;
  diagnostics_.flash_critical_us = metrics.flash_critical_us;
  diagnostics_.dropped_led_frames = metrics.dropped_led_frames;
  diagnostics_.paused_audio_blocks = metrics.paused_audio_blocks;
  diagnostics_.has_persisted_record = service_.has_persisted_record();
  diagnostics_.dirty = service_.dirty();
  diagnostics_.active_differs_from_last_verified =
      !config::equal(service_.active(), service_.last_verified_persisted());
  diagnostics_.save_phase = config::OperationPhase::complete;
  if (reset) {
    diagnostics_.reset_status = diagnostic_status(status);
  } else {
    diagnostics_.save_status = diagnostic_status(status);
  }
  return status;
}
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
  diagnostics_.commit_attempted = false;
  diagnostics_.commit_state_unknown = false;
  diagnostics_.save_phase = config::OperationPhase::validation;
  diagnostics_.validation = config::validate(value);
  if (!diagnostics_.validation)
    return finish(CoordinatorStatus::validation_failed, reset);
  diagnostics_.save_phase = config::OperationPhase::serialization;
  const SaveResult prepared = store_.prepare_save(value);
  if (prepared.status != StoreStatus::ok)
    return finish(CoordinatorStatus::storage_failed, reset);
  diagnostics_.target_slot = prepared.target;
  if (!points_.prepare_activation(value))
    return finish(CoordinatorStatus::activation_failed, reset);
  diagnostics_.save_phase = config::OperationPhase::led_safe_point;
  if (!points_.acquire_led(kLedSafePointDeadlineUs)) {
    points_.restore();
    return finish(CoordinatorStatus::led_timeout, reset);
  }
  diagnostics_.save_phase = config::OperationPhase::audio_safe_point;
  if (!points_.acquire_audio(kAudioSafePointDeadlineUs)) {
    points_.restore();
    return finish(CoordinatorStatus::audio_timeout, reset);
  }
  diagnostics_.save_phase = config::OperationPhase::activation;
  if (!points_.activate_prepared()) {
    points_.restore();
    return finish(CoordinatorStatus::activation_failed, reset);
  }
  service_.activation_succeeded(value);
  diagnostics_.save_phase = config::OperationPhase::erase;
  points_.begin_flash_critical();
  const SaveResult result = store_.commit_prepared();
  points_.end_flash_critical();
  points_.restore();
  diagnostics_.commit_attempted = result.commit_attempted;
  diagnostics_.slot_a = result.a.state;
  diagnostics_.slot_b = result.b.state;
  diagnostics_.slot_a_sequence = result.a.sequence;
  diagnostics_.slot_b_sequence = result.b.sequence;
  diagnostics_.selected_slot = result.selection.selected;
  diagnostics_.selected_sequence = result.selection.sequence;
  diagnostics_.duplicate_sequence = result.selection.duplicate;
  diagnostics_.sequence_ambiguous = result.selection.ambiguous;
  diagnostics_.has_persisted_record = service_.has_persisted_record();
  diagnostics_.schema_status = (result.a.valid() || result.b.valid())
                                   ? config::SchemaStatus::schema1
                                   : config::SchemaStatus::unknown;
  diagnostics_.transaction_phase = diagnostic_phase(result.phase);
  CoordinatorStatus status = CoordinatorStatus::storage_failed;
  if (result.status == StoreStatus::ok) {
    service_.committed_save_succeeded(value);
    diagnostics_.authority = config::AuthorityStatus::target_valid;
    status = CoordinatorStatus::ok;
  } else if (result.status == StoreStatus::commit_state_unknown) {
    diagnostics_.commit_state_unknown = true;
    diagnostics_.authority = config::AuthorityStatus::commit_unknown;
    status = CoordinatorStatus::commit_state_unknown;
  } else if (result.status == StoreStatus::target_invalid) {
    diagnostics_.authority = config::AuthorityStatus::target_invalid;
    status = CoordinatorStatus::target_invalid;
  }
  return finish(status, reset);
}
} // namespace storage
