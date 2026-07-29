#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

// Internal fixed-point FFT backend for the 1,024-sample spectrum window.
// Input samples are signed ADC-centered integer counts. The transform applies
// a Q15 Hann window, then scales every radix-2 stage by one half. Consequently
// the final complex bins are the conventional forward DFT divided by 1,024 and
// remain in signed ADC-count units rather than growing by the FFT length.
namespace spectrum_q15 {

constexpr std::size_t kFftSize = 1024;
constexpr std::size_t kTwiddleCount = kFftSize / 2;

struct ComplexQ15 {
    int16_t real = 0;
    int16_t imaginary = 0;
};

class Backend {
public:
    // Removes the arithmetic mean of the complete input window, applies the
    // flash-resident Q15 Hann table, and executes the in-place radix-2 FFT.
    // The source remains untouched and no dynamic allocation is performed.
    void transform(const std::array<int16_t, kFftSize>& samples);

    // Returns one positive-frequency bin in the stage-scaled FFT domain.
    const ComplexQ15& bin(uint16_t index) const;

    // Returns 2 * (real^2 + imaginary^2) as an unsigned integer. The factor
    // of two is the positive-spectrum correction. The returned value is in
    // squared ADC-count units after the deliberate 1/1,024 FFT stage scale;
    // callers apply Hann-energy/reference calibration when converting to a
    // display level.
    uint64_t positive_bin_energy(uint16_t index) const;

private:
    std::array<ComplexQ15, kFftSize> bins_{};
};

}  // namespace spectrum_q15
