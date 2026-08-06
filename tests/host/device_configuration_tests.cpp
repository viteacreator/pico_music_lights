#include "config/config_boot.hpp"
#include "config/config_service.hpp"
#include "config/crc32.hpp"
#include "config/device_config_codec.hpp"
#include "config/runtime_adapter.hpp"
#include "config/schema_enum_mapping.hpp"
#include "storage/fake_flash_backend.hpp"
#include "storage/save_coordinator.hpp"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      std::cerr << __func__ << ":" << __LINE__ << " CHECK(" #x ") failed\n";   \
      std::exit(1);                                                            \
    }                                                                          \
  } while (0)
using namespace config;
using namespace storage;
void factory_defaults_all_eight_channels() {
  auto v = make_factory_defaults();
  uint16_t n[] = {132, 174, 141, 81, 96, 72};
  for (int i = 0; i < 6; i++) {
    CHECK(v.led_channels[i].enabled);
    CHECK(v.led_channels[i].pixel_count == n[i]);
    CHECK(v.led_channels[i].channel_order == config::ChannelOrder::grbw);
    CHECK(v.led_channels[i].physical.length_mm == 0);
    CHECK(v.led_channels[i].physical.density_pixels_per_metre == 60);
  }
  for (int i = 6; i < 8; i++) {
    CHECK(!v.led_channels[i].enabled);
    CHECK(v.led_channels[i].pixel_count == 0);
    CHECK(v.effects[i].type == effects::EffectType::off);
  }
  CHECK(!v.idle_lighting.enabled);
}
void schema_geometry_and_no_gpio() {
  CHECK(kMaximumLedChannelCount == 8);
  CHECK(kSchema1PayloadSize == 978);
  CHECK(kHeaderSize == 64);
  CHECK(kSlotSize == 4096);
  CHECK(kCommitPageSize == 256);
  CHECK(kCommitPageOffset == 3840);
}
void crc_check_vector() {
  const char *s = "123456789";
  CHECK(crc32((const uint8_t *)s, 9) == 0xcbf43926u);
  CHECK(crc32(nullptr, 0) == 0);
}
void codec_roundtrip_and_malformed() {
  auto v = make_factory_defaults();
  v.led_channels[7].pixel_count = 65535;
  PayloadBuffer b{};
  CHECK(encode(v, b) == CodecStatus::ok);
  DeviceConfiguration out{};
  CHECK(decode(b.data(), b.size(), out) == CodecStatus::ok);
  CHECK(equal(v, out));
  CHECK(decode(b.data(), b.size() - 1, out) == CodecStatus::wrong_length);
  auto bad = b;
  bad[4] = 2;
  CHECK(decode(bad.data(), bad.size(), out) == CodecStatus::malformed);
  bad = b;
  bad[2] = 1;
  CHECK(decode(bad.data(), bad.size(), out) == CodecStatus::reserved_nonzero);
  bad = b;
  bad[932] = 2;
  CHECK(decode(bad.data(), bad.size(), out) == CodecStatus::malformed);
}
void validation_boundaries() {
  auto v = make_factory_defaults();
  v.led_channels[0].pixel_count = 300;
  v.led_channels[1].enabled = false;
  v.led_channels[2].enabled = false;
  v.led_channels[3].enabled = false;
  v.led_channels[4].enabled = false;
  v.led_channels[5].enabled = false;
  CHECK(validate(v));
  v.led_channels[0].pixel_count = 301;
  CHECK(validate(v).error == ValidationError::invalid_pixel_count);
  v.led_channels[0].enabled = false;
  v.led_channels[6].pixel_count = 500;
  CHECK(validate(v));
  v.led_channels[6].enabled = true;
  CHECK(validate(v).error == ValidationError::unsupported_channel);
  v = make_factory_defaults();
  v.idle_lighting.logical_channel_mask |= 0x40;
  CHECK(validate(v).error == ValidationError::invalid_idle);
}
void canonical_calibration_overlay() {
  auto v = make_factory_defaults();
  v.audio_calibration.gyver_left_noise_floor = 77;
  auto r = to_runtime(v.effects[0], v.audio_calibration);
  CHECK(r.gyver_left_noise_floor == 77);
  PayloadBuffer b{};
  CHECK(encode(v, b) == CodecStatus::ok);
  CHECK(b[962] == 77);
}
void record_golden_and_selection() {
  auto v = make_factory_defaults();
  SlotBuffer a{}, b{};
  build_record(v, 0xffffffffu, a);
  build_record(v, 0, b);
  CHECK(a[0] == 'P' && a[1] == 'M' && a[2] == 'L' && a[3] == 'C');
  CHECK(a[3840] == 'C' && a[3841] == 'M' && a[3842] == 'T' && a[3843] == '1');
  auto ia = inspect_record(a.data(), a.size()),
       ib = inspect_record(b.data(), b.size());
  CHECK(ia.valid() && ib.valid());
  CHECK(select_newest(ia, ib).selected == SlotId::b);
  build_record(v, 5, b);
  ia = inspect_record(b.data(), b.size());
  auto dup = select_newest(ia, ia);
  CHECK(dup.selected == SlotId::a && dup.duplicate);
  build_record(v, 0x80000005u, b);
  auto amb = select_newest(ia, inspect_record(b.data(), b.size()));
  CHECK(amb.selected == SlotId::a && amb.ambiguous);
  CHECK(choose_target(amb).slot == SlotId::b);
}
void storage_empty_first_and_alternating() {
  FakeFlashBackend f;
  DeviceConfigStore s(f);
  CHECK(s.load().status == StoreStatus::no_record);
  auto v = make_factory_defaults();
  auto one = s.save(v);
  CHECK(one.status == StoreStatus::ok && one.target == SlotId::a &&
        one.sequence == 0);
  auto two = s.save(v);
  CHECK(two.status == StoreStatus::ok && two.target == SlotId::b &&
        two.sequence == 1);
  CHECK(s.load().selection.selected == SlotId::b);
}
void corruption_and_interruption() {
  auto v = make_factory_defaults();
  for (int cut : {0, 1, 8, 15, 16, 64, 128, 255}) {
    FakeFlashBackend f;
    DeviceConfigStore s(f);
    f.fail_program_after_bytes(cut);
    auto r = s.save(v);
    CHECK(r.status == StoreStatus::precommit_failure ||
          r.status == StoreStatus::target_invalid);
    FakeFlashBackend reboot = f;
    DeviceConfigStore rs(reboot);
    CHECK(rs.load().status == StoreStatus::no_record);
  }
  FakeFlashBackend f;
  DeviceConfigStore s(f);
  CHECK(s.save(v).status == StoreStatus::ok);
  f.bytes()[f.region().persistent_start + 64] ^= 1;
  CHECK(s.load().status == StoreStatus::no_record);
}
struct Points : SafePointController {
  bool led = true, audio = true, activation = true, preparation = true;
  int stage = 0;
  FlashBackend *flash = nullptr;
  bool prepare_activation(const DeviceConfiguration &) override {
    CHECK(stage == 0);
    stage = 1;
    return preparation;
  }
  bool acquire_led(uint32_t) override {
    CHECK(stage == 1);
    stage = 2;
    return led;
  }
  bool acquire_audio(uint32_t) override {
    CHECK(stage == 2);
    stage = 3;
    return audio;
  }
  bool activate_prepared() override {
    CHECK(stage == 3);
    if (flash) {
      auto c = flash->counters();
      CHECK(c.erases == 0 && c.programs == 0);
    }
    stage = 4;
    return activation;
  }
  void restore() override { stage = 5; }
  void reset() {
    led = audio = activation = preparation = true;
    stage = 0;
  }
};
void service_safe_points_and_reset() {
  ConfigService c;
  FakeFlashBackend f;
  DeviceConfigStore s(f);
  Points p;
  config::ConfigurationDiagnostics diagnostics{};
  p.flash = &f;
  SaveCoordinator co(c, s, p, diagnostics);
  p.led = false;
  CHECK(co.save() == CoordinatorStatus::led_timeout);
  CHECK(f.counters().erases == 0);
  p.reset();
  p.flash = &f;
  p.audio = false;
  CHECK(co.save() == CoordinatorStatus::audio_timeout);
  CHECK(f.counters().erases == 0);
  p.reset();
  p.flash = &f;
  CHECK(co.factory_reset(1) == CoordinatorStatus::confirmation_required);
  CHECK(co.save() == CoordinatorStatus::ok);
  CHECK(c.has_persisted_record() && !c.dirty());
}

void exhaustive_commit_page_interruption_and_reboot() {
  const auto defaults = make_factory_defaults();
  for (uint32_t cut = 0u; cut < 256u; ++cut) {
    FakeFlashBackend flash;
    DeviceConfigStore store(flash);
    CHECK(store.prepare_save(defaults).status == StoreStatus::ok);
    flash.set_fault({FakeOperationType::commit, 1u, cut, 0u, UINT32_MAX});
    const SaveResult result = store.commit_prepared();
    FakeFlashBackend reboot_flash = flash;
    reboot_flash.clear_fault();
    DeviceConfigStore reboot(reboot_flash);
    const LoadResult loaded = reboot.load();
    if (cut < 16u) {
      CHECK(result.status == StoreStatus::target_invalid);
      CHECK(loaded.status == StoreStatus::no_record);
    } else {
      CHECK(result.status == StoreStatus::ok);
      CHECK(loaded.status == StoreStatus::ok);
      CHECK(loaded.selection.selected == SlotId::a);
    }
  }
}

void post_commit_unknown_does_not_repair() {
  FakeFlashBackend flash;
  DeviceConfigStore store(flash);
  const auto defaults = make_factory_defaults();
  CHECK(store.prepare_save(defaults).status == StoreStatus::ok);
  // Two preparation reads and one uncommitted target read precede the two
  // independent post-commit reads. Fail the first post-commit read.
  flash.set_fault({FakeOperationType::read, 4u, 0u, 0u, UINT32_MAX});
  const SaveResult result = store.commit_prepared();
  CHECK(result.status == StoreStatus::commit_state_unknown);
  CHECK(result.commit_attempted);
  CHECK(flash.counters().erases == 1u);
  flash.clear_fault();
  DeviceConfigStore reboot(flash);
  CHECK(reboot.load().status == StoreStatus::ok);
  CHECK(flash.counters().erases == 1u);
}

void activation_then_storage_failure_keeps_active_dirty() {
  ConfigService service;
  auto changed = service.draft();
  changed.led_channels[0].brightness = 7u;
  CHECK(service.replace_draft(changed));
  FakeFlashBackend flash;
  DeviceConfigStore store(flash);
  flash.set_fault({FakeOperationType::erase, 1u, 0u, 0u, UINT32_MAX});
  Points points;
  points.flash = &flash;
  config::ConfigurationDiagnostics diagnostics{};
  SaveCoordinator coordinator(service, store, points, diagnostics);
  CHECK(coordinator.save() == CoordinatorStatus::storage_failed);
  CHECK(service.active().led_channels[0].brightness == 7u);
  CHECK(service.last_verified_persisted().led_channels[0].brightness == 16u);
  CHECK(!service.has_persisted_record());
  CHECK(service.dirty());
}

void schema_enum_mapping_is_explicit() {
  uint8_t id = 255u;
  CHECK(config::schema::effect_type_to_id(
      effects::EffectType::gyver_spectrum_analyzer, id));
  CHECK(id == 22u);
  effects::EffectType type = effects::EffectType::off;
  CHECK(config::schema::effect_type_from_id(12u, type));
  CHECK(type == effects::EffectType::gyver_vu_gradient);
  CHECK(!config::schema::effect_type_from_id(23u, type));
}

void disabled_channel_retained_count_runtime_zero() {
  auto value = make_factory_defaults();
  value.led_channels[2].enabled = false;
  value.led_channels[2].pixel_count = 65535u;
  CurrentBoardRuntimeConfiguration runtime{};
  CHECK(adapt_current_board(value, runtime));
  CHECK(!runtime.led[2].enabled);
  CHECK(runtime.led[2].pixel_count == 0u);
  CHECK(value.led_channels[2].pixel_count == 65535u);
  value.led_channels[6].pixel_count = 1234u;
  CHECK(adapt_current_board(value, runtime));
  value.led_channels[6].enabled = true;
  CHECK(!adapt_current_board(value, runtime));
}

void startup_load_and_factory_fallback() {
  FakeFlashBackend empty_flash;
  DeviceConfigStore empty_store(empty_flash);
  ConfigService empty_service;
  config::ConfigurationDiagnostics empty_diagnostics{};
  config::initialize_from_storage(empty_service, empty_store.load(),
                                  empty_flash.region(), empty_diagnostics);
  CHECK(empty_diagnostics.boot_source == config::BootSource::factory_defaults);
  CHECK(!empty_service.has_persisted_record());
  CHECK(!empty_service.dirty());

  FakeFlashBackend saved_flash;
  DeviceConfigStore saved_store(saved_flash);
  auto saved = make_factory_defaults();
  saved.led_channels[0].brightness = 9u;
  CHECK(saved_store.save(saved).status == StoreStatus::ok);
  ConfigService saved_service;
  config::ConfigurationDiagnostics saved_diagnostics{};
  config::initialize_from_storage(saved_service, saved_store.load(),
                                  saved_flash.region(), saved_diagnostics);
  CHECK(saved_diagnostics.boot_source == config::BootSource::persistent);
  CHECK(saved_service.has_persisted_record());
  CHECK(saved_service.active().led_channels[0].brightness == 9u);
  CurrentBoardRuntimeConfiguration runtime{};
  CHECK(adapt_current_board(saved_service.active(), runtime));
  CHECK(runtime.led[0].brightness == 9u);
}

void off_effect_canonicalization_ignores_hidden_fields() {
  auto first = make_factory_defaults();
  auto second = first;
  second.effects[7].primary_color = {1u, 2u, 3u, 4u};
  second.effects[7].attack_ms = 999u;
  PayloadBuffer a{}, b{};
  CHECK(encode(first, a) == CodecStatus::ok);
  CHECK(encode(second, b) == CodecStatus::ok);
  CHECK(a == b);
  CHECK(equal(first, second));
}
void bounded_diagnostics_and_memory_budget() {
  config::ConfigurationDiagnostics diagnostics{};
  diagnostics.boot_source = config::BootSource::factory_defaults;
  diagnostics.flash_region = {16384, 4096, 4096, 12288, 4096, 256};
  std::array<char, config::kDiagnosticTextCapacity> text{};
  const std::size_t length = config::format_diagnostics(diagnostics, text);
  CHECK(length < text.size());
  CHECK(text.back() == '\0');
  CHECK(sizeof(EffectDeviceConfig) <= 128u);
  CHECK(sizeof(DeviceConfiguration) <= 1200u);
  CHECK(sizeof(SlotInspection) <= 1300u);
  CHECK(sizeof(LoadResult) <= 4000u);
  CHECK(DeviceConfigStore::workspace_bytes() == 12544u);
}
void flash_layout() {
  CHECK(valid_region({16384, 4096, 4096, 12288, 4096, 256}));
  CHECK(!valid_region({16384, 5000, 4096, 12288, 4096, 256}));
  CHECK(!valid_region({16384, 0, 4097, 12289, 4096, 256}));
}
int main() {
  factory_defaults_all_eight_channels();
  schema_geometry_and_no_gpio();
  crc_check_vector();
  codec_roundtrip_and_malformed();
  validation_boundaries();
  canonical_calibration_overlay();
  record_golden_and_selection();
  storage_empty_first_and_alternating();
  corruption_and_interruption();
  service_safe_points_and_reset();
  exhaustive_commit_page_interruption_and_reboot();
  post_commit_unknown_does_not_repair();
  activation_then_storage_failure_keeps_active_dirty();
  schema_enum_mapping_is_explicit();
  disabled_channel_retained_count_runtime_zero();
  startup_load_and_factory_fallback();
  off_effect_canonicalization_ignores_hidden_fields();
  bounded_diagnostics_and_memory_budget();
  flash_layout();
  std::cout << "device_configuration_tests: PASS\n";
}
