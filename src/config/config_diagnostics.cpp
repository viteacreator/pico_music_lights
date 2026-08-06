#include "config/config_diagnostics.hpp"
#include <cstdio>
namespace config {
std::size_t format_diagnostics(const ConfigurationDiagnostics &d,
                               std::array<char, kDiagnosticTextCapacity> &out) {
  const int n = std::snprintf(
      out.data(), out.size(),
      "cfg boot=%u fallback=%u schema=%u authority=%u persisted=%u "
      "slots=%u:%lu,%u:%lu selected=%u seq=%lu target=%u dup=%u amb=%u "
      "validation=%u/%u/%u/%u/%u dirty=%u active_diff=%u save_phase=%u "
      "txn_phase=%u "
      "save=%u reset=%u commit=%u unknown=%u waits=%lu,%lu "
      "flash_us=%lu drop=%lu,%lu bounds=%08lx-%08lx app_end=%08lx\n",
      unsigned(d.boot_source), unsigned(d.fallback_reason),
      unsigned(d.schema_status), unsigned(d.authority),
      d.has_persisted_record ? 1u : 0u, unsigned(d.slot_a),
      static_cast<unsigned long>(d.slot_a_sequence), unsigned(d.slot_b),
      static_cast<unsigned long>(d.slot_b_sequence), unsigned(d.selected_slot),
      static_cast<unsigned long>(d.selected_sequence), unsigned(d.target_slot),
      d.duplicate_sequence ? 1u : 0u, d.sequence_ambiguous ? 1u : 0u,
      unsigned(d.validation.error), unsigned(d.validation.section),
      unsigned(d.validation.field), unsigned(d.validation.reason),
      unsigned(d.validation.channel), d.dirty ? 1u : 0u,
      d.active_differs_from_last_verified ? 1u : 0u, unsigned(d.save_phase),
      unsigned(d.transaction_phase), unsigned(d.save_status),
      unsigned(d.reset_status), d.commit_attempted ? 1u : 0u,
      d.commit_state_unknown ? 1u : 0u,
      static_cast<unsigned long>(d.led_safe_wait_us),
      static_cast<unsigned long>(d.audio_safe_wait_us),
      static_cast<unsigned long>(d.flash_critical_us),
      static_cast<unsigned long>(d.dropped_led_frames),
      static_cast<unsigned long>(d.paused_audio_blocks),
      static_cast<unsigned long>(d.flash_region.persistent_start),
      static_cast<unsigned long>(d.flash_region.persistent_end),
      static_cast<unsigned long>(d.flash_region.application_end));
  if (n < 0) {
    out[0] = '\0';
    return 0u;
  }
  return static_cast<std::size_t>(n) >= out.size()
             ? out.size() - 1u
             : static_cast<std::size_t>(n);
}
} // namespace config
