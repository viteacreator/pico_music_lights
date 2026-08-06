#pragma once
#include "config/device_config.hpp"
#include "storage/flash_backend.hpp"
#include "storage/persistent_record.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace config {
enum class BootSource : uint8_t { unknown, factory_defaults, persistent };
enum class OperationPhase : uint8_t {
  idle,
  validation,
  serialization,
  inspection,
  led_safe_point,
  audio_safe_point,
  activation,
  erase,
  data_program,
  uncommitted_verify,
  commit_program,
  committed_inspection,
  restore,
  complete
};
enum class OperationStatus : uint8_t {
  none,
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
struct ConfigurationDiagnostics {
  BootSource boot_source = BootSource::unknown;
  bool has_persisted_record = false;
  storage::SlotState slot_a = storage::SlotState::erased;
  storage::SlotState slot_b = storage::SlotState::erased;
  storage::SlotId selected_slot = storage::SlotId::none;
  storage::SlotId target_slot = storage::SlotId::none;
  uint32_t selected_sequence = 0u;
  bool duplicate_sequence = false;
  bool sequence_ambiguous = false;
  ValidationResult validation{};
  OperationPhase save_phase = OperationPhase::idle;
  OperationStatus save_status = OperationStatus::none;
  OperationStatus reset_status = OperationStatus::none;
  bool commit_attempted = false;
  bool commit_state_unknown = false;
  bool dirty = false;
  bool active_differs_from_last_verified = false;
  uint32_t led_safe_wait_us = 0u;
  uint32_t audio_safe_wait_us = 0u;
  uint32_t flash_critical_us = 0u;
  uint32_t dropped_led_frames = 0u;
  uint32_t paused_audio_blocks = 0u;
  storage::FlashRegion flash_region{};
};
constexpr std::size_t kDiagnosticTextCapacity = 384u;
std::size_t format_diagnostics(const ConfigurationDiagnostics &,
                               std::array<char, kDiagnosticTextCapacity> &);
} // namespace config
