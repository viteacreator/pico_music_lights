#pragma once
#include "storage/flash_backend.hpp"
#include <array>
namespace storage {
class FakeFlashBackend final : public FlashBackend {
public:
  static constexpr std::size_t kCapacity = 16384;
  explicit FakeFlashBackend(FlashRegion r = {16384, 4096, 4096, 12288, 4096,
                                             256});
  FlashStatus read(uint32_t, uint8_t *, std::size_t) override;
  FlashStatus erase(uint32_t, std::size_t) override;
  FlashStatus program(uint32_t, const uint8_t *, std::size_t,
                      bool = false) override;
  FlashRegion region() const override { return region_; }
  OperationCounters counters() const override { return counters_; }
  void fail_read_after(int n) { read_budget_ = n; }
  void fail_erase_after_bytes(int n) { erase_budget_ = n; }
  void fail_program_after_bytes(int n) { program_budget_ = n; }
  uint8_t *bytes() { return bytes_.data(); }

private:
  FlashRegion region_;
  std::array<uint8_t, kCapacity> bytes_{};
  OperationCounters counters_{};
  int read_budget_ = -1, erase_budget_ = -1, program_budget_ = -1;
};
} // namespace storage
