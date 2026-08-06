#pragma once
#include "config/device_config.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
namespace config {
constexpr std::size_t kSchema1PayloadSize = 978;
using PayloadBuffer = std::array<uint8_t, kSchema1PayloadSize>;
enum class CodecStatus : uint8_t {
  ok,
  wrong_length,
  malformed,
  reserved_nonzero,
  validation_failed
};
CodecStatus encode(const DeviceConfiguration &, PayloadBuffer &);
CodecStatus decode(const uint8_t *, std::size_t, DeviceConfiguration &);
} // namespace config
