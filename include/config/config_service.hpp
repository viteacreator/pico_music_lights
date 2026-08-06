#pragma once
#include "config/device_config.hpp"
namespace config {
class ConfigService {
public:
  explicit ConfigService(DeviceConfiguration factory = make_factory_defaults());
  const DeviceConfiguration &factory_defaults() const { return factory_; }
  const DeviceConfiguration &active() const { return active_; }
  const DeviceConfiguration &draft() const { return draft_; }
  const DeviceConfiguration &last_verified_persisted() const {
    return persisted_;
  }
  bool has_persisted_record() const { return has_persisted_; }
  bool dirty() const;
  ValidationResult replace_draft(const DeviceConfiguration &);
  ValidationResult preview();
  void load_persisted(const DeviceConfiguration &);
  void load_factory_fallback();
  void mark_saved();
  void discard();
  bool save_eligible() const { return dirty() || !has_persisted_; }

private:
  DeviceConfiguration factory_, active_, draft_, persisted_;
  bool has_persisted_ = false;
};
} // namespace config
