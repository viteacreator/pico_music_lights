#include "storage/fake_flash_backend.hpp"
#include <algorithm>
#include <cstring>
namespace storage {
FakeFlashBackend::FakeFlashBackend(FlashRegion r) : region_(r) {
  bytes_.fill(0xff);
}
FlashStatus FakeFlashBackend::read(uint32_t o, uint8_t *d, std::size_t n) {
  counters_.reads++;
  if (read_budget_ == 0)
    return FlashStatus::read_failure;
  if (read_budget_ > 0)
    --read_budget_;
  if (o > bytes_.size() || n > bytes_.size() - o)
    return FlashStatus::bounds;
  std::memcpy(d, bytes_.data() + o, n);
  return FlashStatus::ok;
}
FlashStatus FakeFlashBackend::erase(uint32_t o, std::size_t n) {
  counters_.erases++;
  if (o % region_.erase_size || n % region_.erase_size)
    return FlashStatus::alignment;
  if (o > bytes_.size() || n > bytes_.size() - o)
    return FlashStatus::bounds;
  std::size_t done = n;
  if (erase_budget_ >= 0)
    done = std::min(done, (std::size_t)erase_budget_);
  std::fill_n(bytes_.begin() + o, done, 0xff);
  if (done < n)
    return FlashStatus::injected_failure;
  return FlashStatus::ok;
}
FlashStatus FakeFlashBackend::program(uint32_t o, const uint8_t *s,
                                      std::size_t n, bool commit) {
  counters_.programs++;
  if (commit)
    counters_.commit_programs++;
  if (o % region_.program_size || n % region_.program_size)
    return FlashStatus::alignment;
  if (o > bytes_.size() || n > bytes_.size() - o)
    return FlashStatus::bounds;
  std::size_t done = n;
  if (program_budget_ >= 0)
    done = std::min(done, (std::size_t)program_budget_);
  for (std::size_t i = 0; i < done; i++) {
    if ((bytes_[o + i] & s[i]) != s[i])
      return FlashStatus::program_violation;
    bytes_[o + i] &= s[i];
  }
  if (done < n)
    return FlashStatus::injected_failure;
  return FlashStatus::ok;
}
} // namespace storage
