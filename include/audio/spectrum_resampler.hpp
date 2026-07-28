#pragma once
#include "audio/spectrum_analyzer.hpp"
void resample_spectrum(const SpectrumFrame& source,uint16_t* destination,std::size_t segment_count);
