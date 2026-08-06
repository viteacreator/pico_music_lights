#include "config/crc32.hpp"
namespace config {
uint32_t crc32_update(uint32_t crc, const uint8_t *d, std::size_t n) {
  for (std::size_t i = 0; i < n; i++) {
    crc ^= d[i];
    for (int b = 0; b < 8; b++)
      crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0u);
  }
  return crc;
}
uint32_t crc32(const uint8_t *d, std::size_t n) {
  return crc32_update(0xffffffffu, d, n) ^ 0xffffffffu;
}
} // namespace config
