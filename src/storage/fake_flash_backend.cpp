#include "storage/fake_flash_backend.hpp"
#include <algorithm>
#include <cstring>

namespace storage {
namespace {
std::size_t type_index(FakeOperationType type) {
  return static_cast<std::size_t>(type);
}
} // namespace

FakeFlashBackend::FakeFlashBackend(FlashRegion region) : region_(region) {
  bytes_.fill(0xffu);
}

bool FakeFlashBackend::fault_matches(FakeOperationType type, uint32_t offset,
                                     std::size_t length) {
  const uint32_t operation = ++type_counts_[type_index(type)];
  if (fault_.operation_number == 0u || fault_.type != type ||
      fault_.operation_number != operation) {
    return false;
  }
  const uint64_t end = static_cast<uint64_t>(offset) + length;
  const uint64_t fault_end =
      static_cast<uint64_t>(fault_.range_start) + fault_.range_length;
  return offset < fault_end && end > fault_.range_start;
}

void FakeFlashBackend::record(FakeOperationType type, uint32_t offset,
                              uint32_t length, uint32_t completed,
                              FlashStatus status) {
  if (event_count_ < events_.size()) {
    events_[event_count_++] = {type, offset, length, completed, status};
  }
}

FlashStatus FakeFlashBackend::read(uint32_t offset, uint8_t *destination,
                                   std::size_t length) {
  ++counters_.reads;
  if (destination == nullptr || offset > bytes_.size() ||
      length > bytes_.size() - offset) {
    record(FakeOperationType::read, offset, length, 0u, FlashStatus::bounds);
    return FlashStatus::bounds;
  }
  if (fault_matches(FakeOperationType::read, offset, length)) {
    record(FakeOperationType::read, offset, length, 0u,
           FlashStatus::read_failure);
    return FlashStatus::read_failure;
  }
  std::memcpy(destination, bytes_.data() + offset, length);
  record(FakeOperationType::read, offset, length, length, FlashStatus::ok);
  return FlashStatus::ok;
}

FlashStatus FakeFlashBackend::erase(uint32_t offset, std::size_t length) {
  ++counters_.erases;
  if (offset % region_.erase_size != 0u || length == 0u ||
      length % region_.erase_size != 0u) {
    record(FakeOperationType::erase, offset, length, 0u,
           FlashStatus::alignment);
    return FlashStatus::alignment;
  }
  if (offset > bytes_.size() || length > bytes_.size() - offset) {
    record(FakeOperationType::erase, offset, length, 0u, FlashStatus::bounds);
    return FlashStatus::bounds;
  }
  const bool fail = fault_matches(FakeOperationType::erase, offset, length);
  const std::size_t completed =
      fail ? std::min<std::size_t>(length, fault_.bytes_before_failure)
           : length;
  std::fill_n(bytes_.begin() + offset, completed, 0xffu);
  const FlashStatus status =
      completed < length ? FlashStatus::injected_failure : FlashStatus::ok;
  record(FakeOperationType::erase, offset, length, completed, status);
  return status;
}

FlashStatus FakeFlashBackend::program(uint32_t offset, const uint8_t *source,
                                      std::size_t length, bool commit) {
  ++counters_.programs;
  if (commit) {
    ++counters_.commit_programs;
  }
  const FakeOperationType type =
      commit ? FakeOperationType::commit : FakeOperationType::program;
  if (source == nullptr || offset % region_.program_size != 0u ||
      length == 0u || length % region_.program_size != 0u) {
    record(type, offset, length, 0u, FlashStatus::alignment);
    return FlashStatus::alignment;
  }
  if (offset > bytes_.size() || length > bytes_.size() - offset) {
    record(type, offset, length, 0u, FlashStatus::bounds);
    return FlashStatus::bounds;
  }
  const bool fail = fault_matches(type, offset, length);
  const std::size_t completed =
      fail ? std::min<std::size_t>(length, fault_.bytes_before_failure)
           : length;
  for (std::size_t index = 0u; index < completed; ++index) {
    if ((bytes_[offset + index] & source[index]) != source[index]) {
      record(type, offset, length, index, FlashStatus::program_violation);
      return FlashStatus::program_violation;
    }
    bytes_[offset + index] &= source[index];
  }
  const FlashStatus status =
      completed < length ? FlashStatus::injected_failure : FlashStatus::ok;
  record(type, offset, length, completed, status);
  return status;
}

void FakeFlashBackend::fail_read_after(int calls) {
  fault_ = {FakeOperationType::read,
            calls < 0 ? 0u : static_cast<uint32_t>(calls + 1), 0u, 0u,
            UINT32_MAX};
}
void FakeFlashBackend::fail_erase_after_bytes(int bytes) {
  fault_ = {FakeOperationType::erase, 1u,
            bytes < 0 ? UINT32_MAX : static_cast<uint32_t>(bytes), 0u,
            UINT32_MAX};
}
void FakeFlashBackend::fail_program_after_bytes(int bytes) {
  fault_ = {FakeOperationType::program, 1u,
            bytes < 0 ? UINT32_MAX : static_cast<uint32_t>(bytes), 0u,
            UINT32_MAX};
}
} // namespace storage
