#pragma once

#include "audio/spectrum_analyzer.hpp"

#include <cstddef>
#include <cstdint>

// Maps 32 source bands to a caller-owned segment array. Supported output sizes
// are 5, 8, 16, and 32. Invalid arguments leave destination unchanged.
void resample_spectrum(const SpectrumFrame& source,
                       uint16_t* destination,
                       std::size_t segment_count);
