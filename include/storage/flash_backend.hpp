#pragma once
#include <cstddef>
#include <cstdint>
namespace storage {
struct FlashRegion {
  uint32_t flash_size = 0, application_end = 0, persistent_start = 0,
           persistent_end = 0, erase_size = 4096, program_size = 256;
};
enum class FlashStatus : uint8_t {
  ok,
  bounds,
  alignment,
  program_violation,
  injected_failure,
  read_failure
};
struct OperationCounters {
  uint32_t reads = 0, erases = 0, programs = 0, commit_programs = 0;
};
bool valid_region(const FlashRegion &);
class FlashBackend {
public:
  virtual ~FlashBackend() = default;
  virtual FlashStatus read(uint32_t, uint8_t *, std::size_t) = 0;
  virtual FlashStatus erase(uint32_t, std::size_t) = 0;
  virtual FlashStatus program(uint32_t, const uint8_t *, std::size_t,
                              bool commit = false) = 0;
  virtual FlashRegion region() const = 0;
  virtual OperationCounters counters() const = 0;
};
} // namespace storage
