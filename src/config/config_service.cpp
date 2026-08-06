#include "config/config_service.hpp"
namespace config {
ConfigService::ConfigService(DeviceConfiguration f)
    : factory_(f), active_(f), draft_(f), persisted_(f) {}
bool ConfigService::dirty() const {
  return !equal(draft_, persisted_) || !equal(active_, persisted_);
}
ValidationResult ConfigService::replace_draft(const DeviceConfiguration &v) {
  auto r = validate(v);
  if (r)
    draft_ = v;
  return r;
}
ValidationResult ConfigService::preview() {
  auto r = validate(draft_);
  if (r)
    active_ = draft_;
  return r;
}
void ConfigService::load_persisted(const DeviceConfiguration &v) {
  persisted_ = active_ = draft_ = v;
  has_persisted_ = true;
}
void ConfigService::load_factory_fallback() {
  persisted_ = active_ = draft_ = factory_;
  has_persisted_ = false;
}
void ConfigService::mark_saved() {
  persisted_ = active_ = draft_;
  has_persisted_ = true;
}
void ConfigService::discard() {
  draft_ = active_ = has_persisted_ ? persisted_ : factory_;
}
} // namespace config
