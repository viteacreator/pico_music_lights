#include "config/firmware_config_runtime.hpp"
#include "audio/audio_capture.hpp"
#include "led/diagnostic_renderer.hpp"
#include "pico/time.h"

bool FirmwareConfigRuntime::prepare_activation(
    const config::DeviceConfiguration &value) {
  if (!config::validate(value))
    return false;
  prepared_ = value;
  prepared_valid_ = true;
  return true;
}
bool FirmwareConfigRuntime::acquire_led(uint32_t deadline_us) {
  renderer_was_enabled_ = diagnostic_renderer_is_enabled();
  if (renderer_was_enabled_ && (!diagnostic_renderer_set_enabled(false)))
    return false;
  led_acquired_ = diagnostic_renderer_wait_until_idle(deadline_us);
  if (!led_acquired_)
    restore();
  return led_acquired_;
}
bool FirmwareConfigRuntime::acquire_audio(uint32_t) {
  if (!led_acquired_) {
    return false;
  }
  // A ready block is stable RAM and does not prevent pausing the currently
  // filling DMA. The synchronous Save caller cannot drain it while waiting.
  audio_acquired_ = audio_capture_pause();
  return audio_acquired_;
}

bool FirmwareConfigRuntime::activate_prepared() {
  if (!prepared_valid_ || !led_acquired_ || !audio_acquired_)
    return false;
  return diagnostic_renderer_publish_configuration(prepared_);
}
void FirmwareConfigRuntime::restore() {
  if (audio_acquired_) {
    audio_capture_resume();
  }
  if (renderer_was_enabled_)
    (void)diagnostic_renderer_set_enabled(true);
  prepared_valid_ = false;
  led_acquired_ = false;
  audio_acquired_ = false;
  renderer_was_enabled_ = false;
}
