#include "audio/spectrum_q15_backend.hpp"

#include <cstdint>
#include <limits>

namespace spectrum_q15 {
namespace {

// The generated const tables have internal linkage and are read directly from
// flash on Pico builds. They intentionally avoid runtime trigonometry and
// per-frame coefficient generation.
#include "spectrum_q15_tables.inc"

int16_t saturate_i16(int32_t value) {
    if (value > std::numeric_limits<int16_t>::max()) {
        return std::numeric_limits<int16_t>::max();
    }

    if (value < std::numeric_limits<int16_t>::min()) {
        return std::numeric_limits<int16_t>::min();
    }

    return static_cast<int16_t>(value);
}

int32_t round_shift_q15(int32_t value) {
    constexpr int32_t kRounding = 1 << 14;

    if (value >= 0) {
        return static_cast<int32_t>((value + kRounding) >> 15);
    }

    return -static_cast<int32_t>((-value + kRounding) >> 15);
}

int16_t halve_with_rounding(int32_t value) {
    // Round to nearest with ties to even. In particular, +1 / 2 and -1 / 2
    // both become zero. The prior ties-away-from-zero rule retained +/-1
    // butterfly residues at every stage, which could accumulate as artificial
    // broadband energy for very small inputs.
    const bool negative = value < 0;
    const uint32_t magnitude = negative
                                   ? static_cast<uint32_t>(-value)
                                   : static_cast<uint32_t>(value);
    uint32_t rounded = magnitude >> 1u;

    if ((magnitude & 1u) != 0u && (rounded & 1u) != 0u) {
        ++rounded;
    }

    const int32_t signed_rounded = negative
                                       ? -static_cast<int32_t>(rounded)
                                       : static_cast<int32_t>(rounded);
    return saturate_i16(signed_rounded);
}

int32_t multiply_q15(int16_t coefficient, int16_t value) {
    return round_shift_q15(
        static_cast<int32_t>(coefficient) * static_cast<int32_t>(value));
}

ComplexQ15 multiply_complex_q15(ComplexQ15 coefficient, ComplexQ15 value) {
    const int32_t real = multiply_q15(coefficient.real, value.real) -
                         multiply_q15(coefficient.imaginary, value.imaginary);
    const int32_t imaginary = multiply_q15(coefficient.real, value.imaginary) +
                              multiply_q15(coefficient.imaginary, value.real);

    return {
        saturate_i16(real),
        saturate_i16(imaginary),
    };
}

void bit_reverse(std::array<ComplexQ15, kFftSize>& bins) {
    for (uint16_t index = 1, reversed = 0; index < kFftSize; ++index) {
        uint16_t bit = static_cast<uint16_t>(kFftSize >> 1u);

        while ((reversed & bit) != 0u) {
            reversed = static_cast<uint16_t>(reversed ^ bit);
            bit = static_cast<uint16_t>(bit >> 1u);
        }

        reversed = static_cast<uint16_t>(reversed ^ bit);

        if (index < reversed) {
            const ComplexQ15 temporary = bins[index];
            bins[index] = bins[reversed];
            bins[reversed] = temporary;
        }
    }
}

void execute_fft(std::array<ComplexQ15, kFftSize>& bins) {
    bit_reverse(bins);

    for (uint16_t length = 2; length <= kFftSize; length <<= 1u) {
        const uint16_t half_length = static_cast<uint16_t>(length >> 1u);
        const uint16_t twiddle_step = static_cast<uint16_t>(kFftSize / length);

        for (uint16_t block = 0; block < kFftSize; block += length) {
            for (uint16_t offset = 0; offset < half_length; ++offset) {
                const uint16_t even_index = static_cast<uint16_t>(block + offset);
                const uint16_t odd_index =
                    static_cast<uint16_t>(even_index + half_length);
                const ComplexQ15 transformed = multiply_complex_q15(
                    kTwiddlesQ15[offset * twiddle_step], bins[odd_index]);
                const ComplexQ15 even = bins[even_index];

                // Every stage divides both butterfly outputs by two. This
                // prevents radix-2 growth and gives a total FFT scale of
                // 1/1,024, keeping the final bins in input-count units.
                bins[even_index] = {
                    halve_with_rounding(
                        static_cast<int32_t>(even.real) + transformed.real),
                    halve_with_rounding(
                        static_cast<int32_t>(even.imaginary) + transformed.imaginary),
                };
                bins[odd_index] = {
                    halve_with_rounding(
                        static_cast<int32_t>(even.real) - transformed.real),
                    halve_with_rounding(
                        static_cast<int32_t>(even.imaginary) - transformed.imaginary),
                };
            }
        }
    }
}

}  // namespace

void Backend::transform(const std::array<int16_t, kFftSize>& samples) {
    int64_t sum = 0;

    for (int16_t sample : samples) {
        sum += sample;
    }

    const int32_t mean = static_cast<int32_t>(sum / static_cast<int64_t>(kFftSize));

    for (std::size_t index = 0; index < kFftSize; ++index) {
        const int32_t centered = static_cast<int32_t>(samples[index]) - mean;
        const int32_t windowed = round_shift_q15(
            centered * static_cast<int32_t>(kHannQ15[index]));

        bins_[index] = {
            saturate_i16(windowed),
            0,
        };
    }

    execute_fft(bins_);
}

const ComplexQ15& Backend::bin(uint16_t index) const {
    return bins_[index];
}

uint64_t Backend::positive_bin_energy(uint16_t index) const {
    const ComplexQ15& value = bins_[index];
    const int64_t real = value.real;
    const int64_t imaginary = value.imaginary;

    return 2u * static_cast<uint64_t>(real * real + imaginary * imaginary);
}

}  // namespace spectrum_q15
