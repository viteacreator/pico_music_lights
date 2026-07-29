#include "audio/spectrum_resampler.hpp"

namespace {

bool supported_segment_count(std::size_t segment_count) {
    return segment_count == 5 || segment_count == 8 ||
           segment_count == 16 || segment_count == kSpectrumBandCount;
}

}  // namespace

void resample_spectrum(const SpectrumFrame& source,
                       uint16_t* destination,
                       std::size_t segment_count) {
    if (destination == nullptr || !supported_segment_count(segment_count)) {
        return;
    }

    for (std::size_t destination_index = 0;
         destination_index < segment_count;
         ++destination_index) {
        uint32_t weighted_sum = 0;
        uint32_t total_weight = 0;
        const std::size_t scaled_first = destination_index * kSpectrumBandCount;
        const std::size_t scaled_last =
            (destination_index + 1u) * kSpectrumBandCount;

        for (std::size_t scaled_source = scaled_first;
             scaled_source < scaled_last;
             ++scaled_source) {
            const std::size_t source_index = scaled_source / segment_count;
            ++total_weight;
            weighted_sum += source.bands[source_index];
        }

        destination[destination_index] =
            static_cast<uint16_t>(weighted_sum / total_weight);
    }
}
