#include "storage/rp2040_flash_backend.hpp"
#include "hardware/flash.h"
#include "pico/flash.h"
#include "pico/stdlib.h"
#include "storage/persistent_record.hpp"

#include <cstring>

#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (2u * 1024u * 1024u)
#endif

extern "C" uint8_t __flash_binary_end;

namespace storage {
namespace {
constexpr uint32_t kPersistentBytes = 8192u;
constexpr uint32_t kFlashSafetyTimeoutMs = 100u;
static_assert(kPersistentBytes % FLASH_SECTOR_SIZE == 0u);
static_assert(kSlotSize % FLASH_SECTOR_SIZE == 0u);
static_assert(kCommitPageSize == FLASH_PAGE_SIZE);

struct FlashCommand {
  uint32_t offset;
  const uint8_t *data;
  std::size_t length;
  bool erase;
};

bool within_one_slot(const FlashRegion &region, uint32_t offset,
                     std::size_t length) {
  if (offset < region.persistent_start || offset > region.persistent_end ||
      length > region.persistent_end - offset) {
    return false;
  }
  const uint32_t slot_offset = offset - region.persistent_start;
  const uint32_t slot_index = slot_offset / kSlotSize;
  const uint32_t slot_end =
      region.persistent_start + (slot_index + 1u) * kSlotSize;
  return slot_index < 2u && offset + length <= slot_end;
}

void execute_flash_command(void *context) {
  auto *const command = static_cast<FlashCommand *>(context);
  if (command->erase) {
    flash_range_erase(command->offset, command->length);
  } else {
    flash_range_program(command->offset, command->data, command->length);
  }
}
} // namespace

Rp2040FlashBackend::Rp2040FlashBackend() {
  const uintptr_t image_end_address =
      reinterpret_cast<uintptr_t>(&__flash_binary_end);
  const uint32_t image_end =
      image_end_address >= XIP_BASE ? image_end_address - XIP_BASE : UINT32_MAX;
  region_ = {PICO_FLASH_SIZE_BYTES,
             image_end,
             PICO_FLASH_SIZE_BYTES - kPersistentBytes,
             PICO_FLASH_SIZE_BYTES,
             FLASH_SECTOR_SIZE,
             FLASH_PAGE_SIZE};
}

FlashStatus Rp2040FlashBackend::read(uint32_t offset, uint8_t *destination,
                                     std::size_t length) {
  ++counters_.reads;
  if (destination == nullptr || !within_one_slot(region_, offset, length)) {
    return FlashStatus::bounds;
  }
  std::memcpy(destination, reinterpret_cast<const void *>(XIP_BASE + offset),
              length);
  return FlashStatus::ok;
}

FlashStatus Rp2040FlashBackend::erase(uint32_t offset, std::size_t length) {
  ++counters_.erases;
  if (offset % FLASH_SECTOR_SIZE != 0u || length == 0u ||
      length % FLASH_SECTOR_SIZE != 0u) {
    return FlashStatus::alignment;
  }
  if (!within_one_slot(region_, offset, length)) {
    return FlashStatus::bounds;
  }
  FlashCommand command{offset, nullptr, length, true};
  return flash_safe_execute(execute_flash_command, &command,
                            kFlashSafetyTimeoutMs) == PICO_OK
             ? FlashStatus::ok
             : FlashStatus::injected_failure;
}

FlashStatus Rp2040FlashBackend::program(uint32_t offset, const uint8_t *data,
                                        std::size_t length, bool commit) {
  ++counters_.programs;
  if (commit) {
    ++counters_.commit_programs;
  }
  if (data == nullptr || offset % FLASH_PAGE_SIZE != 0u || length == 0u ||
      length % FLASH_PAGE_SIZE != 0u) {
    return FlashStatus::alignment;
  }
  if (!within_one_slot(region_, offset, length)) {
    return FlashStatus::bounds;
  }
  FlashCommand command{offset, data, length, false};
  return flash_safe_execute(execute_flash_command, &command,
                            kFlashSafetyTimeoutMs) == PICO_OK
             ? FlashStatus::ok
             : FlashStatus::injected_failure;
}
} // namespace storage
