#include "storage/device_config_store.hpp"
#include <algorithm>
namespace storage {
uint32_t DeviceConfigStore::offset(SlotId s) const {
  return backend_.region().persistent_start + (s == SlotId::b ? kSlotSize : 0);
}
bool DeviceConfigStore::read_slot(SlotId s, SlotBuffer &o) {
  return backend_.read(offset(s), o.data(), o.size()) == FlashStatus::ok;
}
LoadResult DeviceConfigStore::load() {
  LoadResult r{};
  if (!valid_region(backend_.region())) {
    r.status = StoreStatus::invalid_region;
    return r;
  }
  SlotBuffer a, b;
  if (!read_slot(SlotId::a, a) || !read_slot(SlotId::b, b)) {
    r.status = StoreStatus::read_failure;
    return r;
  }
  r.a = inspect_record(a.data(), a.size());
  r.b = inspect_record(b.data(), b.size());
  r.selection = select_newest(r.a, r.b);
  if (r.selection.selected == SlotId::none) {
    r.status = StoreStatus::no_record;
    return r;
  }
  r.value = r.selection.selected == SlotId::a ? r.a.value : r.b.value;
  r.status = StoreStatus::ok;
  return r;
}
SaveResult DeviceConfigStore::save(const config::DeviceConfiguration &v) {
  SaveResult r{};
  auto before = load();
  if (before.status == StoreStatus::read_failure ||
      before.status == StoreStatus::invalid_region) {
    r.status = before.status;
    return r;
  }
  auto target = choose_target(before.selection);
  r.target = target.slot;
  r.sequence = target.sequence;
  SlotBuffer image;
  build_record(v, target.sequence, image);
  uint32_t base = offset(target.slot);
  if (backend_.erase(base, kSlotSize) != FlashStatus::ok)
    return r;
  std::array<uint8_t, 256> page{};
  for (uint32_t at = 0; at < kCommitPageOffset; at += 256) {
    page.fill(0xff);
    uint32_t count =
        std::min<uint32_t>(256, kHeaderSize + config::kSchema1PayloadSize - at);
    if (at >= kHeaderSize + config::kSchema1PayloadSize)
      break;
    std::copy_n(image.data() + at, count, page.data());
    if (backend_.program(base + at, page.data(), page.size()) !=
        FlashStatus::ok)
      return r;
  }
  SlotBuffer verify;
  if (!read_slot(target.slot, verify) ||
      inspect_record(verify.data(), verify.size(), false).state !=
          SlotState::valid)
    return r;
  r.commit_attempted = true;
  if (backend_.program(base + kCommitPageOffset,
                       image.data() + kCommitPageOffset, kCommitPageSize,
                       true) != FlashStatus::ok) {
  }
  auto after = load();
  if (after.status == StoreStatus::read_failure) {
    r.status = StoreStatus::commit_state_unknown;
    return r;
  }
  const auto &t = target.slot == SlotId::a ? after.a : after.b;
  if (!t.valid()) {
    r.status = StoreStatus::target_invalid;
    return r;
  }
  if (after.selection.selected != target.slot) {
    r.status = StoreStatus::target_invalid;
    return r;
  }
  r.status = StoreStatus::ok;
  return r;
}
} // namespace storage
