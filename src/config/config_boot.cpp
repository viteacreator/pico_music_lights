#include "config/config_boot.hpp"
namespace config {
void initialize_from_storage(ConfigService &service,
                             const storage::LoadResult &load,
                             const storage::FlashRegion &region,
                             ConfigurationDiagnostics &diagnostics) {
  if (load.status == storage::StoreStatus::ok) {
    service.load_persisted(load.value);
    diagnostics.boot_source = BootSource::persistent;
    diagnostics.fallback_reason = FallbackReason::none;
    diagnostics.schema_status = SchemaStatus::schema1;
    diagnostics.authority = AuthorityStatus::selected_valid;
  } else {
    service.load_factory_fallback();
    diagnostics.boot_source = BootSource::factory_defaults;
    diagnostics.authority = AuthorityStatus::none;
    if (load.status == storage::StoreStatus::read_failure) {
      diagnostics.fallback_reason = FallbackReason::read_failure;
    } else if (load.status == storage::StoreStatus::invalid_region) {
      diagnostics.fallback_reason = FallbackReason::invalid_region;
    } else if (load.a.state == storage::SlotState::erased &&
               load.b.state == storage::SlotState::erased) {
      diagnostics.fallback_reason = FallbackReason::empty;
    } else if (load.a.state == storage::SlotState::unsupported_schema ||
               load.b.state == storage::SlotState::unsupported_schema) {
      diagnostics.fallback_reason = FallbackReason::unsupported_schema;
      diagnostics.schema_status = SchemaStatus::unsupported;
    } else {
      diagnostics.fallback_reason = FallbackReason::both_invalid;
      diagnostics.schema_status = SchemaStatus::corrupt;
    }
  }
  diagnostics.has_persisted_record = service.has_persisted_record();
  diagnostics.slot_a = load.a.state;
  diagnostics.slot_b = load.b.state;
  diagnostics.selected_slot = load.selection.selected;
  diagnostics.selected_sequence = load.selection.sequence;
  diagnostics.slot_a_sequence = load.a.sequence;
  diagnostics.slot_b_sequence = load.b.sequence;
  diagnostics.duplicate_sequence = load.selection.duplicate;
  diagnostics.sequence_ambiguous = load.selection.ambiguous;
  diagnostics.dirty = service.dirty();
  diagnostics.active_differs_from_last_verified =
      !equal(service.active(), service.last_verified_persisted());
  diagnostics.flash_region = region;
}
} // namespace config
