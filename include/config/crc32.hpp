#pragma once
#include <cstddef>
#include <cstdint>
namespace config {
uint32_t crc32(const uint8_t *, std::size_t);
uint32_t crc32_update(uint32_t, const uint8_t *, std::size_t);
} // namespace config
