#include "config/config_boot.hpp"
namespace config {
void initialize_from_storage(ConfigService &service,
                             const storage::LoadResult &load,
                             const storage::FlashRegion &region,
                             ConfigurationDiagnostics &diagnostics) {
  if (load.status == storage::StoreStatus::ok) {
    service.load_persisted(load.value);
    diagnostics.boot_source = BootSource::persistent;
  } else {
    service.load_factory_fallback();
    diagnostics.boot_source = BootSource::factory_defaults;
  }
  diagnostics.has_persisted_record = service.has_persisted_record();
  diagnostics.slot_a = load.a.state;
  diagnostics.slot_b = load.b.state;
  diagnostics.selected_slot = load.selection.selected;
  diagnostics.selected_sequence = load.selection.sequence;
  diagnostics.duplicate_sequence = load.selection.duplicate;
  diagnostics.sequence_ambiguous = load.selection.ambiguous;
  diagnostics.dirty = service.dirty();
  diagnostics.active_differs_from_last_verified =
      !equal(service.active(), service.last_verified_persisted());
  diagnostics.flash_region = region;
}
} // namespace config
