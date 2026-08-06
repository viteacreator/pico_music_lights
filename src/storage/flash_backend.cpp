#include "storage/flash_backend.hpp"
namespace storage {
bool valid_region(const FlashRegion &r) {
  return r.flash_size && r.erase_size && r.program_size &&
         r.persistent_start >= r.application_end &&
         r.persistent_start < r.persistent_end &&
         r.persistent_end <= r.flash_size &&
         r.persistent_start % r.erase_size == 0 &&
         r.persistent_end % r.erase_size == 0 &&
         (r.persistent_end - r.persistent_start) == 8192;
}
} // namespace storage
