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
enum class StorePhase : uint8_t {
  none,
  inspection,
  erase,
  data_program,
  uncommitted_verify,
  commit_program,
  committed_inspection
};
struct SaveResult {
  StoreStatus status = StoreStatus::precommit_failure;
  SlotId target = SlotId::none;
  uint32_t sequence = 0;
  bool commit_attempted = false;
  SlotInspection a{}, b{};
  Selection selection{};
  StorePhase phase = StorePhase::none;
};

// One store owns its large fixed workspace. Save/load are deliberately
// non-reentrant; no 4 KiB record buffers are placed on the call stack.
class DeviceConfigStore {
public:
  explicit DeviceConfigStore(FlashBackend &backend) : backend_(backend) {}
  LoadResult load();
  SaveResult prepare_save(const config::DeviceConfiguration &configuration);
  SaveResult commit_prepared();
  SaveResult save(const config::DeviceConfiguration &configuration);
  bool has_prepared_save() const { return prepared_; }
  static constexpr std::size_t workspace_bytes() {
    return sizeof(SlotBuffer) * 3u + 256u + sizeof(LoadResult);
  }

private:
  FlashBackend &backend_;
  SlotBuffer slot_a_{};
  SlotBuffer slot_b_{};
  SlotBuffer write_image_{};
  std::array<uint8_t, 256> program_page_{};
  LoadResult inspection_{};
  WriteTarget prepared_target_{};
  bool prepared_ = false;

  bool read_slot(SlotId slot, SlotBuffer &destination);
  SlotInspection inspect_slot(SlotId slot, SlotBuffer &destination);
  uint32_t offset(SlotId slot) const;
  const LoadResult &inspect_both();
};
static_assert(DeviceConfigStore::workspace_bytes() == 15512u);
} // namespace storage
