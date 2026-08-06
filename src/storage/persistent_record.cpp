#include "storage/persistent_record.hpp"
#include "config/crc32.hpp"
#include <algorithm>
namespace storage {
namespace {
void p16(uint8_t *p, uint16_t v) {
  p[0] = v;
  p[1] = v >> 8;
}
void p32(uint8_t *p, uint32_t v) {
  p16(p, v);
  p16(p + 2, v >> 16);
}
uint16_t g16(const uint8_t *p) { return p[0] | uint16_t(p[1]) << 8; }
uint32_t g32(const uint8_t *p) { return g16(p) | uint32_t(g16(p + 2)) << 16; }
uint32_t hcrc(const uint8_t *p) {
  uint32_t c = config::crc32_update(0xffffffff, p, 56);
  c = config::crc32_update(c, p + 60, 4);
  return c ^ 0xffffffffu;
}
} // namespace
void build_record(const config::DeviceConfiguration &v, uint32_t seq,
                  SlotBuffer &o) {
  o.fill(0xff);
  config::PayloadBuffer p{};
  config::encode(v, p);
  o[0] = 'P';
  o[1] = 'M';
  o[2] = 'L';
  o[3] = 'C';
  p16(o.data() + 4, 1);
  p16(o.data() + 6, 1);
  p16(o.data() + 8, 64);
  p16(o.data() + 10, 0);
  p32(o.data() + 12, p.size());
  p32(o.data() + 16, seq);
  p32(o.data() + 20, config::crc32(p.data(), p.size()));
  p32(o.data() + 24, kDataRegionSize);
  p32(o.data() + 28, kSlotSize);
  p32(o.data() + 32, kCommitPageOffset);
  std::fill(o.begin() + 36, o.begin() + 56, 0);
  std::fill(o.begin() + 60, o.begin() + 64, 0);
  p32(o.data() + 56, hcrc(o.data()));
  std::copy(p.begin(), p.end(), o.begin() + 64);
  auto *c = o.data() + kCommitPageOffset;
  c[0] = 'C';
  c[1] = 'M';
  c[2] = 'T';
  c[3] = '1';
  p32(c + 4, seq);
  p32(c + 8, g32(o.data() + 20));
  p32(c + 12, p.size());
}
SlotInspection inspect_record(const uint8_t *d, std::size_t n, bool commit) {
  SlotInspection r{};
  if (n != kSlotSize) {
    r.state = SlotState::bad_length;
    return r;
  }
  if (std::all_of(d, d + n, [](uint8_t x) { return x == 0xff; })) {
    r.state = SlotState::erased;
    return r;
  }
  if (d[0] != 'P' || d[1] != 'M' || d[2] != 'L' || d[3] != 'C' ||
      g16(d + 4) != 1 || g16(d + 8) != 64) {
    r.state = SlotState::bad_header;
    return r;
  }
  if (g16(d + 6) != 1) {
    r.state = SlotState::unsupported_schema;
    return r;
  }
  if (g16(d + 10) || g32(d + 12) != config::kSchema1PayloadSize ||
      g32(d + 24) != kDataRegionSize || g32(d + 28) != kSlotSize ||
      g32(d + 32) != kCommitPageOffset) {
    r.state = SlotState::bad_length;
    return r;
  }
  for (int i = 36; i < 56; i++)
    if (d[i]) {
      r.state = SlotState::bad_header;
      return r;
    }
  for (int i = 60; i < 64; i++)
    if (d[i]) {
      r.state = SlotState::bad_header;
      return r;
    }
  if (hcrc(d) != g32(d + 56)) {
    r.state = SlotState::bad_header;
    return r;
  }
  if (config::crc32(d + 64, config::kSchema1PayloadSize) != g32(d + 20)) {
    r.state = SlotState::bad_crc;
    return r;
  }
  if (commit) {
    auto *c = d + kCommitPageOffset;
    if (c[0] != 'C' || c[1] != 'M' || c[2] != 'T' || c[3] != '1' ||
        g32(c + 4) != g32(d + 16) || g32(c + 8) != g32(d + 20) ||
        g32(c + 12) != g32(d + 12)) {
      r.state = SlotState::bad_commit;
      return r;
    }
    for (uint32_t i = 16; i < kCommitPageSize; i++)
      if (c[i] != 0xff) {
        r.state = SlotState::bad_commit;
        return r;
      }
  }
  if (config::decode(d + 64, config::kSchema1PayloadSize, r.value) !=
      config::CodecStatus::ok) {
    r.state = SlotState::bad_payload;
    return r;
  }
  r.sequence = g32(d + 16);
  r.state = SlotState::valid;
  return r;
}
Selection select_newest(const SlotInspection &a, const SlotInspection &b) {
  Selection s{};
  if (a.valid() && !b.valid()) {
    s.selected = SlotId::a;
    s.sequence = a.sequence;
  } else if (b.valid() && !a.valid()) {
    s.selected = SlotId::b;
    s.sequence = b.sequence;
  } else if (a.valid() && b.valid()) {
    s.selected = SlotId::a;
    s.sequence = a.sequence;
    uint32_t d = a.sequence - b.sequence;
    if (d == 0)
      s.duplicate = true;
    else if (d == 0x80000000u)
      s.ambiguous = true;
    else if (d > 0x80000000u) {
      s.selected = SlotId::b;
      s.sequence = b.sequence;
    }
  }
  return s;
}
WriteTarget choose_target(const Selection &s) {
  if (s.selected == SlotId::none)
    return {};
  return {s.selected == SlotId::a ? SlotId::b : SlotId::a, s.sequence + 1u};
}
} // namespace storage
