#include "config/config_service.hpp"
namespace config {
ConfigService::ConfigService(DeviceConfiguration factory)
    : factory_(factory), active_(factory), draft_(factory),
      persisted_(factory) {}
bool ConfigService::dirty() const {
  return !equal(draft_, persisted_) || !equal(active_, persisted_);
}
ValidationResult
ConfigService::replace_draft(const DeviceConfiguration &value) {
  const ValidationResult result = validate(value);
  if (result) {
    draft_ = value;
  }
  return result;
}
void ConfigService::activation_succeeded(const DeviceConfiguration &value) {
  active_ = value;
  draft_ = value;
}
void ConfigService::load_persisted(const DeviceConfiguration &value) {
  persisted_ = active_ = draft_ = value;
  has_persisted_ = true;
}
void ConfigService::load_factory_fallback() {
  persisted_ = active_ = draft_ = factory_;
  has_persisted_ = false;
}
void ConfigService::committed_save_succeeded(const DeviceConfiguration &value) {
  persisted_ = value;
  has_persisted_ = true;
}
const DeviceConfiguration &ConfigService::discard_target() const {
  return has_persisted_ ? persisted_ : factory_;
}
} // namespace config
