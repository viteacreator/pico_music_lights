#pragma once
#include "storage/flash_backend.hpp"
namespace storage {
class Rp2040FlashBackend final : public FlashBackend {
public:
  Rp2040FlashBackend();
  FlashStatus read(uint32_t, uint8_t *, std::size_t) override;
  FlashStatus erase(uint32_t, std::size_t) override;
  FlashStatus program(uint32_t, const uint8_t *, std::size_t,
                      bool = false) override;
  FlashRegion region() const override { return region_; }
  OperationCounters counters() const override { return counters_; }

private:
  FlashRegion region_{};
  OperationCounters counters_{};
};
} // namespace storage
