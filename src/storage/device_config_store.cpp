#include "storage/device_config_store.hpp"

#include <algorithm>

namespace storage {
uint32_t DeviceConfigStore::offset(SlotId slot) const {
  return backend_.region().persistent_start +
         (slot == SlotId::b ? kSlotSize : 0u);
}

bool DeviceConfigStore::read_slot(SlotId slot, SlotBuffer &destination) {
  return backend_.read(offset(slot), destination.data(), destination.size()) ==
         FlashStatus::ok;
}

SlotInspection DeviceConfigStore::inspect_slot(SlotId slot,
                                               SlotBuffer &destination) {
  if (!read_slot(slot, destination)) {
    SlotInspection result{};
    result.state = SlotState::read_error;
    return result;
  }
  return inspect_record(destination.data(), destination.size());
}

const LoadResult &DeviceConfigStore::inspect_both() {
  inspection_ = {};
  LoadResult &result = inspection_;
  if (!valid_region(backend_.region())) {
    result.status = StoreStatus::invalid_region;
    return result;
  }
  result.a = inspect_slot(SlotId::a, slot_a_);
  result.b = inspect_slot(SlotId::b, slot_b_);
  if (result.a.state == SlotState::read_error ||
      result.b.state == SlotState::read_error) {
    result.status = StoreStatus::read_failure;
    return result;
  }
  result.selection = select_newest(result.a, result.b);
  if (result.selection.selected == SlotId::none) {
    result.status = StoreStatus::no_record;
    return result;
  }
  result.value =
      result.selection.selected == SlotId::a ? result.a.value : result.b.value;
  result.status = StoreStatus::ok;
  return result;
}

LoadResult DeviceConfigStore::load() { return inspect_both(); }

SaveResult DeviceConfigStore::prepare_save(
    const config::DeviceConfiguration &configuration) {
  prepared_ = false;
  SaveResult result{};
  if (!config::validate(configuration)) {
    return result;
  }
  const LoadResult &before = inspect_both();
  result.a = before.a;
  result.b = before.b;
  result.selection = before.selection;
  result.phase = StorePhase::inspection;
  if (before.status == StoreStatus::read_failure ||
      before.status == StoreStatus::invalid_region) {
    result.status = before.status;
    return result;
  }
  prepared_target_ = choose_target(before.selection);
  build_record(configuration, prepared_target_.sequence, write_image_);
  result.target = prepared_target_.slot;
  result.sequence = prepared_target_.sequence;
  result.status = StoreStatus::ok;
  prepared_ = true;
  return result;
}

SaveResult DeviceConfigStore::commit_prepared() {
  SaveResult result{};
  if (!prepared_) {
    return result;
  }
  result.target = prepared_target_.slot;
  result.sequence = prepared_target_.sequence;
  result.a = inspection_.a;
  result.b = inspection_.b;
  result.selection = inspection_.selection;
  const auto classify_precommit_failure = [&]() {
    const LoadResult &current = inspect_both();
    result.a = current.a;
    result.b = current.b;
    result.selection = current.selection;
    if (current.status == StoreStatus::read_failure)
      result.status = StoreStatus::read_failure;
  };
  const uint32_t base = offset(prepared_target_.slot);
  result.phase = StorePhase::erase;
  if (backend_.erase(base, kSlotSize) != FlashStatus::ok) {
    classify_precommit_failure();
    prepared_ = false;
    return result;
  }

  for (uint32_t at = 0u; at < kHeaderSize + config::kSchema1PayloadSize;
       at += 256u) {
    result.phase = StorePhase::data_program;
    program_page_.fill(0xffu);
    const uint32_t remaining = kHeaderSize + config::kSchema1PayloadSize - at;
    const uint32_t count = std::min<uint32_t>(program_page_.size(), remaining);
    std::copy_n(write_image_.data() + at, count, program_page_.data());
    if (backend_.program(base + at, program_page_.data(),
                         program_page_.size()) != FlashStatus::ok) {
      classify_precommit_failure();
      prepared_ = false;
      return result;
    }
  }

  SlotBuffer &target_buffer =
      prepared_target_.slot == SlotId::a ? slot_a_ : slot_b_;
  result.phase = StorePhase::uncommitted_verify;
  if (!read_slot(prepared_target_.slot, target_buffer) ||
      inspect_record(target_buffer.data(), target_buffer.size(), false).state !=
          SlotState::valid) {
    classify_precommit_failure();
    prepared_ = false;
    return result;
  }

  result.commit_attempted = true;
  result.phase = StorePhase::commit_program;
  (void)backend_.program(base + kCommitPageOffset,
                         write_image_.data() + kCommitPageOffset,
                         kCommitPageSize, true);
  prepared_ = false;

  const LoadResult &after = inspect_both();
  result.phase = StorePhase::committed_inspection;
  result.a = after.a;
  result.b = after.b;
  result.selection = after.selection;
  if (after.status == StoreStatus::read_failure) {
    result.status = StoreStatus::commit_state_unknown;
    return result;
  }
  const SlotInspection &target =
      prepared_target_.slot == SlotId::a ? after.a : after.b;
  if (!target.valid() || after.selection.selected != prepared_target_.slot) {
    result.status = StoreStatus::target_invalid;
    return result;
  }
  result.status = StoreStatus::ok;
  return result;
}

SaveResult
DeviceConfigStore::save(const config::DeviceConfiguration &configuration) {
  SaveResult result = prepare_save(configuration);
  return result.status == StoreStatus::ok ? commit_prepared() : result;
}
} // namespace storage
