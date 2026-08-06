#pragma once
#include "storage/save_coordinator.hpp"
class FirmwareConfigRuntime final : public storage::SafePointController {
public:
  bool prepare_activation(const config::DeviceConfiguration &) override;
  bool acquire_led(uint32_t deadline_us) override;
  bool acquire_audio(uint32_t deadline_us) override;
  bool activate_prepared() override;
  void restore() override;

private:
  config::DeviceConfiguration prepared_{};
  bool prepared_valid_ = false;
  bool renderer_was_enabled_ = false;
  bool led_acquired_ = false;
  bool audio_acquired_ = false;
};
