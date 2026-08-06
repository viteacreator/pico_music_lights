#include "config/firmware_config_runtime.hpp"
#include "audio/audio_capture.hpp"
#include "led/diagnostic_renderer.hpp"
#include "pico/time.h"

bool FirmwareConfigRuntime::prepare_activation(
    const config::DeviceConfiguration &value) {
  metrics_ = {};
  paused_before_ = audio_capture_paused_blocks();
  if (!config::validate(value))
    return false;
  prepared_ = value;
  prepared_valid_ = true;
  return true;
}
bool FirmwareConfigRuntime::acquire_led(uint32_t deadline_us) {
  const uint64_t start = time_us_64();
  renderer_was_enabled_ = diagnostic_renderer_is_enabled();
  if (renderer_was_enabled_ && (!diagnostic_renderer_set_enabled(false)))
    return false;
  led_acquired_ = diagnostic_renderer_wait_until_idle(deadline_us);
  metrics_.led_wait_us = static_cast<uint32_t>(time_us_64() - start);
  if (!led_acquired_)
    restore();
  return led_acquired_;
}
bool FirmwareConfigRuntime::acquire_audio(uint32_t) {
  const uint64_t start = time_us_64();
  if (!led_acquired_) {
    return false;
  }
  // A ready block is stable RAM and does not prevent pausing the currently
  // filling DMA. The synchronous Save caller cannot drain it while waiting.
  audio_acquired_ = audio_capture_pause();
  metrics_.audio_wait_us = static_cast<uint32_t>(time_us_64() - start);
  return audio_acquired_;
}

bool FirmwareConfigRuntime::activate_prepared() {
  if (!prepared_valid_ || !led_acquired_)
    return false;
  return diagnostic_renderer_publish_configuration(prepared_);
}
void FirmwareConfigRuntime::begin_flash_critical() {
  flash_started_us_ = time_us_64();
}
void FirmwareConfigRuntime::end_flash_critical() {
  metrics_.flash_critical_us =
      static_cast<uint32_t>(time_us_64() - flash_started_us_);
}

void FirmwareConfigRuntime::restore() {
  if (audio_acquired_) {
    audio_capture_resume();
  }
  metrics_.paused_audio_blocks = audio_capture_paused_blocks() - paused_before_;
  if (renderer_was_enabled_)
    (void)diagnostic_renderer_set_enabled(true);
  prepared_valid_ = false;
  led_acquired_ = false;
  audio_acquired_ = false;
  renderer_was_enabled_ = false;
}
