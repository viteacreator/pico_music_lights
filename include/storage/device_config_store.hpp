#pragma once
#include "storage/flash_backend.hpp"
#include "storage/persistent_record.hpp"
namespace storage {
enum class StoreStatus : uint8_t {
  ok,
  no_record,
  invalid_region,
  read_failure,
  precommit_failure,
  target_invalid,
  commit_state_unknown
};
struct LoadResult {
  StoreStatus status = StoreStatus::no_record;
  Selection selection{};
  SlotInspection a{}, b{};
  config::DeviceConfiguration value{};
};
struct SaveResult {
  StoreStatus status = StoreStatus::precommit_failure;
  SlotId target = SlotId::none;
  uint32_t sequence = 0;
  bool commit_attempted = false;
};
class DeviceConfigStore {
public:
  explicit DeviceConfigStore(FlashBackend &b) : backend_(b) {}
  LoadResult load();
  SaveResult save(const config::DeviceConfiguration &);

private:
  FlashBackend &backend_;
  bool read_slot(SlotId, SlotBuffer &);
  uint32_t offset(SlotId) const;
};
} // namespace storage
