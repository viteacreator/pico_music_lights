#include "config/config_boot.hpp"
#include "config/config_service.hpp"
#include "config/crc32.hpp"
#include "config/device_config_codec.hpp"
#include "config/runtime_adapter.hpp"
#include "config/runtime_publication.hpp"
#include "config/schema_enum_mapping.hpp"
#include "storage/fake_flash_backend.hpp"
#include "storage/save_coordinator.hpp"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <type_traits>
#include <utility>
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
  OperationCounters before{};
  bool prepare_activation(const DeviceConfiguration &) override {
    CHECK(stage == 0);
    stage = 1;
    if (flash)
      before = flash->counters();
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
    CHECK(stage == 2 || stage == 3);
    if (flash) {
      auto c = flash->counters();
      CHECK(c.erases == before.erases && c.programs == before.programs);
    }
    stage = 4;
    return activation;
  }
  void begin_flash_critical() override {}
  void end_flash_critical() override {}
  void restore() override { stage = 5; }
  SafePointMetrics metrics() const override { return {11u, 12u, 13u, 2u, 3u}; }
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
  CHECK(DeviceConfigStore::workspace_bytes() == 15512u);
}
void flash_layout() {
  CHECK(valid_region({16384, 4096, 4096, 12288, 4096, 256}));
  CHECK(!valid_region({16384, 5000, 4096, 12288, 4096, 256}));
  CHECK(!valid_region({16384, 0, 4097, 12289, 4096, 256}));
}

void canonicalization_covers_every_effect_type() {
  for (uint8_t id = 0u; id <= 22u; ++id) {
    effects::EffectType type{};
    CHECK(config::schema::effect_type_from_id(id, type));
    EffectDeviceConfig value{};
    value.type = type;
    value.enabled = type != effects::EffectType::off;
    for (uint8_t source = 0u; source <= 11u; ++source) {
      effects::EffectSource candidate{};
      if (config::schema::effect_source_from_id(source, candidate) &&
          effects::effect_supports_source(type, candidate)) {
        value.source = candidate;
        break;
      }
    }
    const EffectDeviceConfig canonical = canonicalize_effect(value);
    const auto *metadata = effects::effect_metadata(type);
    CHECK(metadata != nullptr);
    const auto verify_hidden = [&](bool applicable, const auto &mutate) {
      if (applicable)
        return;
      EffectDeviceConfig changed = value;
      mutate(changed);
      DeviceConfiguration first = make_factory_defaults();
      DeviceConfiguration second = first;
      first.effects[7] = canonical;
      second.effects[7] = canonicalize_effect(changed);
      PayloadBuffer a{}, b{};
      CHECK(encode(first, a) == CodecStatus::ok);
      CHECK(encode(second, b) == CodecStatus::ok);
      CHECK(a == b);
    };
    const uint32_t mask = metadata->parameter_mask;
    verify_hidden(mask & effects::effect_parameter_source,
                  [](auto &v) { v.source = effects::EffectSource::aux; });
    verify_hidden(mask & effects::effect_parameter_direction,
                  [](auto &v) { v.reversed = !v.reversed; });
    verify_hidden(mask & effects::effect_parameter_colours, [](auto &v) {
      v.primary_color = {1, 2, 3, 4};
      v.secondary_color = {5, 6, 7, 8};
      v.background_color = {9, 10, 11, 12};
    });
    verify_hidden(mask & effects::effect_parameter_palette,
                  [](auto &v) { v.palette[0] = {1, 2, 3, 4}; });
    verify_hidden(mask & effects::effect_parameter_gain, [](auto &v) {
      v.visual_gain = 777;
      v.background_brightness_q8 = 123;
    });
    verify_hidden(mask & effects::effect_parameter_response, [](auto &v) {
      v.attack_ms = 111;
      v.release_ms = 222;
      v.fade_decay_ms = 333;
    });
    verify_hidden(mask & effects::effect_parameter_animation, [](auto &v) {
      v.animation_speed_q8 = 999;
      v.color_spacing_q8 = 888;
      v.gyver_animation_interval_ms = 77;
      v.gyver_rainbow_span_percent = 66;
    });
    verify_hidden(mask & effects::effect_parameter_geometry, [](auto &v) {
      v.segment_count = 8;
      v.zone_count = 7;
      v.macro_region_count = 3;
    });
    verify_hidden(mask & effects::effect_parameter_strobe, [](auto &v) {
      v.strobe_frequency_hz = 9;
      v.strobe_duty_percent = 44;
      v.strobe_fade_ms = 55;
      v.strobe_envelope_mode = effects::StrobeEnvelopeMode::fade_envelope;
    });
    verify_hidden(mask & effects::effect_parameter_gyver_adaptive, [](auto &v) {
      v.auto_gain_enabled = !v.auto_gain_enabled;
      v.auto_gain_headroom_q8 = 777;
      v.adaptive_fast_response_ms = 111;
      v.adaptive_average_response_ms = 222;
      v.adaptive_trigger_percent = 333;
      v.adaptive_event_decay_ms = 444;
      v.auto_gain_reference_rise_ms = 555;
      v.auto_gain_reference_fall_ms = 666;
    });
    verify_hidden(mask & effects::effect_parameter_static_white_boost,
                  [](auto &v) {
                    v.static_color_mode = effects::StaticColorMode::white_boost;
                    v.white_drive_percent = 150;
                    v.rgb_assist_color = {1, 2, 3, 0};
                  });
    verify_hidden(mask & effects::effect_parameter_comet, [](auto &v) {
      v.frequency_comet_tail_percent = 99;
      v.frequency_comet_quiet_threshold = 123;
    });
    const bool frequency_colours =
        type == effects::EffectType::gyver_frequency_5_zones ||
        type == effects::EffectType::gyver_frequency_3_zones ||
        type == effects::EffectType::gyver_frequency_full_strip ||
        type == effects::EffectType::gyver_running_frequencies;
    verify_hidden(frequency_colours,
                  [](auto &v) { v.gyver_frequency_colors[0] = {1, 2, 3, 4}; });
    const bool vu_mode = type == effects::EffectType::scalar_vu ||
                         type == effects::EffectType::stereo_center_out_vu ||
                         type == effects::EffectType::gyver_vu_gradient ||
                         type == effects::EffectType::gyver_vu_rainbow;
    verify_hidden(vu_mode, [](auto &v) {
      v.vu_color_mode = effects::VuColorMode::level_position_gradient;
    });
    verify_hidden(mask & effects::effect_parameter_frequency_selection,
                  [](auto &v) {
                    v.frequency_selection = effects::FrequencySelection::low;
                  });
    verify_hidden(mask & effects::effect_parameter_running_policy, [](auto &v) {
      v.gyver_running_frequencies_selection =
          effects::GyverFullStripSelectionPolicy::strongest_event;
    });
    verify_hidden(mask & effects::effect_parameter_full_strip_policy,
                  [](auto &v) {
                    v.gyver_full_strip_selection =
                        effects::GyverFullStripSelectionPolicy::strongest_event;
                  });
    verify_hidden(mask & effects::effect_parameter_macro_mapping, [](auto &v) {
      v.macro_band_mapping = effects::GenericMacroBandMapping::bass_mid_high;
    });
  }
}

void validation_named_boundaries() {
  auto value = make_factory_defaults();
  value.led_channels[1].enabled = value.led_channels[2].enabled = false;
  value.led_channels[3].enabled = value.led_channels[4].enabled = false;
  value.led_channels[5].enabled = false;
  value.led_channels[0].pixel_count = 300u;
  CHECK(validate(value));
  value.led_channels[0].pixel_count = 301u;
  CHECK(validate(value).field == ValidationField::pixel_count);
  value = make_factory_defaults();
  for (auto &channel : value.led_channels)
    channel.enabled = false;
  value.led_channels[0].enabled = true;
  value.led_channels[0].pixel_count = 300u;
  value.led_channels[1].enabled = true;
  value.led_channels[1].pixel_count = 300u;
  value.led_channels[2].enabled = true;
  value.led_channels[2].pixel_count = 200u;
  CHECK(validate(value));
  value.led_channels[2].pixel_count = 201u;
  CHECK(validate(value).field == ValidationField::total_pixel_count);
  value = make_factory_defaults();
  value.idle_lighting.silence_timeout_ms = 60001u;
  CHECK(validate(value).field == ValidationField::silence_timeout_ms);
  value = make_factory_defaults();
  value.idle_lighting.left_activity_floor = 2048u;
  CHECK(validate(value).field == ValidationField::left_activity_floor);
  value = make_factory_defaults();
  value.audio_calibration.gyver_left_noise_floor = 2048u;
  CHECK(validate(value).field == ValidationField::gyver_left_noise_floor);
  value = make_factory_defaults();
  value.effects[0].visual_gain = 1025u;
  CHECK(validate(value).field == ValidationField::visual_gain);
  value = make_factory_defaults();
  value.effects[0].attack_ms = 5001u;
  CHECK(validate(value).field == ValidationField::attack_ms);
  value = make_factory_defaults();
  value.effects[0].release_ms = 5001u;
  CHECK(validate(value).field == ValidationField::release_ms);
  value = make_factory_defaults();
  value.idle_lighting.audio_confirmation_ms = 60001u;
  CHECK(validate(value).field == ValidationField::audio_confirmation_ms);
  value = make_factory_defaults();
  value.audio_calibration.gyver_right_noise_floor = 2048u;
  CHECK(validate(value).field == ValidationField::gyver_right_noise_floor);
  value = make_factory_defaults();
  EffectDeviceConfig strobe{};
  strobe.type = effects::EffectType::gyver_stroboscope;
  strobe = canonicalize_effect(strobe);
  strobe.strobe_envelope_mode = effects::StrobeEnvelopeMode::fade_envelope;
  value.effects[0] = strobe;
  CHECK(validate(value).field == ValidationField::strobe_envelope_mode);
  value = make_factory_defaults();
  EffectDeviceConfig adaptive{};
  adaptive.type = effects::EffectType::gyver_spectrum_analyzer;
  adaptive = canonicalize_effect(adaptive);
  adaptive.source = effects::EffectSource::spectrum_32;
  adaptive.auto_gain_reference_rise_ms = 0u;
  value.effects[0] = adaptive;
  CHECK(validate(value).field == ValidationField::auto_gain_reference_rise_ms);
}

void record_crc_reserved_and_padding_coverage() {
  const auto defaults = make_factory_defaults();
  SlotBuffer record{};
  build_record(defaults, 7u, record);
  CHECK(inspect_record(record.data(), record.size()).valid());
  const auto original = record;
  for (std::size_t offset :
       std::array<std::size_t, 7>{0u, 10u, 36u, 55u, 60u, 64u, 500u}) {
    record = original;
    record[offset] ^= 1u;
    CHECK(!inspect_record(record.data(), record.size()).valid());
  }
  record = original;
  record[2000] = 0u;
  CHECK(inspect_record(record.data(), record.size()).valid());
  record = original;
  record[kCommitPageOffset + 16u] = 0u;
  CHECK(inspect_record(record.data(), record.size()).state ==
        SlotState::bad_commit);
  PayloadBuffer payload{};
  CHECK(encode(defaults, payload) == CodecStatus::ok);
  DeviceConfiguration decoded{};
  for (std::size_t offset :
       std::array<std::size_t, 4>{184u, 185u, 958u, 972u}) {
    auto bad = payload;
    bad[offset] = 1u;
    CHECK(decode(bad.data(), bad.size(), decoded) != CodecStatus::ok);
  }
}

void previous_slot_authority_outcomes() {
  auto old_value = make_factory_defaults();
  auto new_value = old_value;
  new_value.led_channels[0].brightness = 8u;
  {
    FakeFlashBackend flash;
    DeviceConfigStore store(flash);
    CHECK(store.save(old_value).status == StoreStatus::ok);
    CHECK(store.prepare_save(new_value).status == StoreStatus::ok);
    flash.set_fault({FakeOperationType::commit, 2u, 8u, 0u, UINT32_MAX});
    CHECK(store.commit_prepared().status == StoreStatus::target_invalid);
    flash.clear_fault();
    const LoadResult reboot = store.load();
    CHECK(reboot.status == StoreStatus::ok);
    CHECK(reboot.selection.selected == SlotId::a);
    CHECK(reboot.value.led_channels[0].brightness == 16u);
  }
  {
    FakeFlashBackend flash;
    DeviceConfigStore store(flash);
    CHECK(store.save(old_value).status == StoreStatus::ok);
    ConfigService service;
    service.load_persisted(old_value);
    CHECK(service.replace_draft(new_value));
    Points points;
    points.flash = &flash;
    ConfigurationDiagnostics diagnostics{};
    SaveCoordinator coordinator(service, store, points, diagnostics);
    flash.set_fault({FakeOperationType::read, 9u, 0u,
                     flash.region().persistent_start, kSlotSize});
    CHECK(coordinator.save() == CoordinatorStatus::commit_state_unknown);
    CHECK(service.has_persisted_record());
    CHECK(service.last_verified_persisted().led_channels[0].brightness == 16u);
    CHECK(service.active().led_channels[0].brightness == 8u);
    CHECK(service.dirty());
    CHECK(diagnostics.authority == AuthorityStatus::commit_unknown);
    flash.clear_fault();
    CHECK(store.load().status == StoreStatus::ok);
  }
}

void cumulative_and_address_targeted_flash_faults() {
  FakeFlashBackend flash;
  std::array<uint8_t, 256> page{};
  page.fill(0u);
  const uint32_t base = flash.region().persistent_start;
  flash.set_fault(
      {FakeOperationType::program, 0u, 255u, base + 256u, 256u, 2u, 300u});
  CHECK(flash.program(base, page.data(), page.size()) == FlashStatus::ok);
  CHECK(flash.program(base + 256u, page.data(), page.size()) ==
        FlashStatus::injected_failure);
  CHECK(flash.event(1).completed == 44u);
  flash.clear_fault();
  flash.reset_fault_tracking();
  CHECK(flash.program(base + 512u, page.data(), page.size()) ==
        FlashStatus::ok);
}

void diagnostics_named_authority_timing_and_fallback() {
  FakeFlashBackend flash;
  DeviceConfigStore store(flash);
  ConfigService service;
  ConfigurationDiagnostics diagnostics{};
  initialize_from_storage(service, store.load(), flash.region(), diagnostics);
  CHECK(diagnostics.fallback_reason == FallbackReason::empty);
  Points points;
  points.flash = &flash;
  SaveCoordinator coordinator(service, store, points, diagnostics);
  CHECK(coordinator.save() == CoordinatorStatus::ok);
  CHECK(diagnostics.led_safe_wait_us == 11u);
  CHECK(diagnostics.audio_safe_wait_us == 12u);
  CHECK(diagnostics.flash_critical_us == 13u);
  CHECK(diagnostics.paused_audio_blocks == 3u);
}

struct PublicationFake : config::RuntimePublicationBackend {
  bool prepare_ok = true, claim_ok = true, led_ok = true, effect_ok = true,
       rollback_ok = true;
  uint32_t claims = 0u, rollbacks = 0u, safe_disables = 0u;
  std::array<bool, board::kStripCount> claimed{};
  bool prepare(const DeviceConfiguration &) override { return prepare_ok; }
  bool acquire_new_resources(const DeviceConfiguration &v) override {
    if (!claim_ok)
      return false;
    for (std::size_t i = 0; i < claimed.size(); ++i)
      if (v.led_channels[i].enabled && !claimed[i]) {
        claimed[i] = true;
        ++claims;
      }
    return true;
  }
  bool switch_led(const DeviceConfiguration &) override { return led_ok; }
  bool switch_effects_idle(const DeviceConfiguration &) override {
    return effect_ok;
  }
  bool rollback(const DeviceConfiguration &) override {
    ++rollbacks;
    return rollback_ok;
  }
  void safe_disable() override { ++safe_disables; }
};
void structural_claim_rollback_safe_disable_and_no_leak() {
  PublicationFake fake;
  config::RuntimePublicationCoordinator publisher(fake);
  auto previous = make_factory_defaults();
  auto next = previous;
  CHECK(publisher.publish(previous, next) == config::PublicationStatus::ok);
  const uint32_t first_claims = fake.claims;
  next.led_channels[1].enabled = false;
  CHECK(publisher.publish(previous, next) == config::PublicationStatus::ok);
  next.led_channels[1].enabled = true;
  CHECK(publisher.publish(previous, next) == config::PublicationStatus::ok);
  CHECK(fake.claims == first_claims);
  fake.claim_ok = false;
  CHECK(publisher.publish(previous, next) ==
        config::PublicationStatus::claim_failed);
  fake.claim_ok = true;
  fake.effect_ok = false;
  CHECK(publisher.publish(previous, next) ==
        config::PublicationStatus::effect_switch_failed);
  CHECK(fake.rollbacks == 1u);
  fake.rollback_ok = false;
  CHECK(publisher.publish(previous, next) ==
        config::PublicationStatus::rollback_failed_safe_disabled);
  CHECK(fake.safe_disables == 1u);
}

template <typename T, typename = void>
struct has_gpio_member : std::false_type {};
template <typename T>
struct has_gpio_member<T, std::void_t<decltype(std::declval<T>().gpio)>>
    : std::true_type {};
void exact_golden_bytes_for_every_record_class_and_no_gpio() {
  static_assert(!has_gpio_member<LedChannelDeviceConfig>::value);
  const auto value = make_factory_defaults();
  PayloadBuffer payload{};
  CHECK(encode(value, payload) == CodecStatus::ok);
  const std::array<uint8_t, 10> led0 = {1u,  132u, 0u, 1u,  0u,
                                        16u, 0u,   0u, 60u, 0u};
  CHECK(std::equal(led0.begin(), led0.end(), payload.begin() + 4));
  CHECK(payload[84] == 1u);
  CHECK(payload[85] == 12u);
  CHECK(payload[86] == 9u);
  CHECK(payload[99] == 0u && payload[100] == 255u && payload[101] == 0u &&
        payload[102] == 0u);
  CHECK(payload[932] == 0u);
  CHECK(payload[933] == 1u);
  CHECK(payload[948] == effects::kIdleAllInputs);
  CHECK(payload[949] == 0x3fu);
  CHECK(payload[962] == 32u && payload[963] == 0u);
  CHECK(payload[970] == 64u && payload[971] == 0u);
  SlotBuffer record{};
  build_record(value, 0x01020304u, record);
  const std::array<uint8_t, 20> header_prefix = {
      'P', 'M', 'L',   'C',   1u, 0u, 1u, 0u, 64u, 0u,
      0u,  0u,  0xd2u, 0x03u, 0u, 0u, 4u, 3u, 2u,  1u};
  CHECK(std::equal(header_prefix.begin(), header_prefix.end(), record.begin()));
  CHECK(record[3840] == 'C' && record[3841] == 'M' && record[3842] == 'T' &&
        record[3843] == '1');
}

void malformed_boolean_enum_reserved_truncation_and_extra_bytes() {
  const auto value = make_factory_defaults();
  PayloadBuffer payload{};
  CHECK(encode(value, payload) == CodecStatus::ok);
  DeviceConfiguration decoded{};
  for (std::size_t offset : std::array<std::size_t, 3>{4u, 8u, 932u}) {
    auto bad = payload;
    bad[offset] = 2u;
    CHECK(decode(bad.data(), bad.size(), decoded) != CodecStatus::ok);
  }
  for (std::pair<std::size_t, uint8_t> mutation :
       std::array<std::pair<std::size_t, uint8_t>, 4>{
           {{7u, 2u}, {85u, 23u}, {86u, 12u}, {115u, 2u}}}) {
    auto bad = payload;
    bad[mutation.first] = mutation.second;
    CHECK(decode(bad.data(), bad.size(), decoded) != CodecStatus::ok);
  }
  CHECK(decode(payload.data(), payload.size() - 1u, decoded) ==
        CodecStatus::wrong_length);
  std::array<uint8_t, kSchema1PayloadSize + 1u> extra{};
  std::copy(payload.begin(), payload.end(), extra.begin());
  CHECK(decode(extra.data(), extra.size(), decoded) ==
        CodecStatus::wrong_length);
}

void factory_reset_failure_phases_and_authority() {
  FakeFlashBackend flash;
  DeviceConfigStore store(flash);
  ConfigService service;
  auto persisted = make_factory_defaults();
  persisted.led_channels[0].brightness = 5u;
  CHECK(store.save(persisted).status == StoreStatus::ok);
  service.load_persisted(persisted);
  Points points;
  points.flash = &flash;
  ConfigurationDiagnostics diagnostics{};
  SaveCoordinator coordinator(service, store, points, diagnostics);
  CHECK(coordinator.factory_reset(0u) ==
        CoordinatorStatus::confirmation_required);
  flash.set_fault({FakeOperationType::erase, 2u, 0u, 0u, UINT32_MAX});
  CHECK(coordinator.factory_reset(SaveCoordinator::kFactoryResetConfirmation) ==
        CoordinatorStatus::storage_failed);
  CHECK(service.active().led_channels[0].brightness == 16u);
  CHECK(service.last_verified_persisted().led_channels[0].brightness == 5u);
  CHECK(service.dirty());
  flash.clear_fault();
  points.reset();
  points.flash = &flash;
  CHECK(coordinator.reload() == CoordinatorStatus::ok);
  CHECK(service.active().led_channels[0].brightness == 5u);
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
  canonicalization_covers_every_effect_type();
  validation_named_boundaries();
  record_crc_reserved_and_padding_coverage();
  previous_slot_authority_outcomes();
  cumulative_and_address_targeted_flash_faults();
  diagnostics_named_authority_timing_and_fallback();
  structural_claim_rollback_safe_disable_and_no_leak();
  exact_golden_bytes_for_every_record_class_and_no_gpio();
  malformed_boolean_enum_reserved_truncation_and_extra_bytes();
  factory_reset_failure_phases_and_authority();
  flash_layout();
  std::cout << "device_configuration_tests: PASS\n";
}
