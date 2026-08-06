#pragma once
#include "config/device_config_codec.hpp"
#include <array>
#include <cstdint>
namespace storage {
constexpr uint32_t kSlotSize = 4096, kDataRegionSize = 3840,
                   kCommitPageOffset = 3840, kCommitPageSize = 256,
                   kHeaderSize = 64;
enum class SlotId : uint8_t { a, b, none };
enum class SlotState : uint8_t {
  valid,
  erased,
  bad_header,
  unsupported_schema,
  bad_length,
  bad_crc,
  bad_commit,
  bad_payload,
  read_error
};
struct SlotInspection {
  SlotState state = SlotState::erased;
  uint32_t sequence = 0;
  config::DeviceConfiguration value{};
  bool valid() const { return state == SlotState::valid; }
};
struct Selection {
  SlotId selected = SlotId::none;
  bool duplicate = false, ambiguous = false;
  uint32_t sequence = 0;
};
using SlotBuffer = std::array<uint8_t, kSlotSize>;
void build_record(const config::DeviceConfiguration &, uint32_t, SlotBuffer &);
SlotInspection inspect_record(const uint8_t *, std::size_t,
                              bool require_commit = true);
Selection select_newest(const SlotInspection &, const SlotInspection &);
struct WriteTarget {
  SlotId slot = SlotId::a;
  uint32_t sequence = 0;
};
WriteTarget choose_target(const Selection &);
} // namespace storage
