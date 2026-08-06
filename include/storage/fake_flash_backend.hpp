#pragma once
#include "storage/flash_backend.hpp"
#include <array>

namespace storage {
enum class FakeOperationType : uint8_t { read, erase, program, commit };
struct FakeFault {
  FakeOperationType type = FakeOperationType::read;
  uint32_t operation_number = 0u; // zero disables the fault
  uint32_t bytes_before_failure = 0u;
  uint32_t range_start = 0u;
  uint32_t range_length = UINT32_MAX;
  uint32_t global_operation_number = 0u;
  uint64_t cumulative_bytes_before_failure = UINT64_MAX;
};
struct FakeOperationEvent {
  FakeOperationType type = FakeOperationType::read;
  uint32_t offset = 0u;
  uint32_t length = 0u;
  uint32_t completed = 0u;
  FlashStatus status = FlashStatus::ok;
};

class FakeFlashBackend final : public FlashBackend {
public:
  static constexpr std::size_t kCapacity = 16384u;
  static constexpr std::size_t kMaximumEvents = 128u;
  explicit FakeFlashBackend(FlashRegion region = {16384, 4096, 4096, 12288,
                                                  4096, 256});
  FlashStatus read(uint32_t, uint8_t *, std::size_t) override;
  FlashStatus erase(uint32_t, std::size_t) override;
  FlashStatus program(uint32_t, const uint8_t *, std::size_t,
                      bool = false) override;
  FlashRegion region() const override { return region_; }
  OperationCounters counters() const override { return counters_; }

  void set_fault(FakeFault fault) { fault_ = fault; }
  void clear_fault() { fault_ = {}; }
  void reset_fault_tracking() {
    type_counts_.fill(0u);
    global_operation_count_ = 0u;
    cumulative_bytes_ = 0u;
  }
  void fail_read_after(int calls);
  void fail_erase_after_bytes(int bytes);
  void fail_program_after_bytes(int bytes);
  uint8_t *bytes() { return bytes_.data(); }
  const uint8_t *bytes() const { return bytes_.data(); }
  std::size_t event_count() const { return event_count_; }
  const FakeOperationEvent &event(std::size_t index) const {
    return events_[index];
  }

private:
  FlashRegion region_;
  std::array<uint8_t, kCapacity> bytes_{};
  OperationCounters counters_{};
  FakeFault fault_{};
  std::array<uint32_t, 4> type_counts_{};
  std::array<FakeOperationEvent, kMaximumEvents> events_{};
  std::size_t event_count_ = 0u;
  uint32_t global_operation_count_ = 0u;
  uint64_t cumulative_bytes_ = 0u;

  std::size_t completed_before_fault(FakeOperationType, uint32_t, std::size_t);
  void record(FakeOperationType, uint32_t, uint32_t, uint32_t, FlashStatus);
};
} // namespace storage
