#include "storage/rp2040_flash_backend.hpp"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"
#include "storage/persistent_record.hpp"
#include <cstring>
#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (2u * 1024u * 1024u)
#endif
namespace storage {
namespace {
constexpr uint32_t kPersistentBytes = 8192;
static_assert(kPersistentBytes % FLASH_SECTOR_SIZE == 0);
static_assert(kSlotSize % FLASH_SECTOR_SIZE == 0);
} // namespace
Rp2040FlashBackend::Rp2040FlashBackend() {
  region_ = {PICO_FLASH_SIZE_BYTES,
             0,
             PICO_FLASH_SIZE_BYTES - kPersistentBytes,
             PICO_FLASH_SIZE_BYTES,
             FLASH_SECTOR_SIZE,
             FLASH_PAGE_SIZE};
}
FlashStatus Rp2040FlashBackend::read(uint32_t o, uint8_t *d, std::size_t n) {
  counters_.reads++;
  if (o > region_.flash_size || n > region_.flash_size - o)
    return FlashStatus::bounds;
  std::memcpy(d, reinterpret_cast<const void *>(XIP_BASE + o), n);
  return FlashStatus::ok;
}
FlashStatus Rp2040FlashBackend::erase(uint32_t o, std::size_t n) {
  counters_.erases++;
  if (o % FLASH_SECTOR_SIZE || n % FLASH_SECTOR_SIZE)
    return FlashStatus::alignment;
  uint32_t irq = save_and_disable_interrupts();
  flash_range_erase(o, n);
  restore_interrupts(irq);
  return FlashStatus::ok;
}
FlashStatus Rp2040FlashBackend::program(uint32_t o, const uint8_t *d,
                                        std::size_t n, bool commit) {
  counters_.programs++;
  if (commit)
    counters_.commit_programs++;
  if (o % FLASH_PAGE_SIZE || n % FLASH_PAGE_SIZE)
    return FlashStatus::alignment;
  uint32_t irq = save_and_disable_interrupts();
  flash_range_program(o, d, n);
  restore_interrupts(irq);
  return FlashStatus::ok;
}
} // namespace storage
