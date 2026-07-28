#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
constexpr std::size_t kSpectrumBandCount=32, kSpectrumWindowSamples=1024, kCenteredMonoBlockSamples=256;
struct CenteredMonoBlock { std::array<int16_t,kCenteredMonoBlockSamples> samples{}; uint32_t sequence=0; };
struct SpectrumFrame { std::array<uint16_t,kSpectrumBandCount> bands{}; uint16_t bass=0,low=0,mid=0,high=0; uint32_t sequence=0,analysis_time_us=0,maximum_analysis_time_us=0,dropped_windows=0; };
class SpectrumAnalyzer { public: bool push(const CenteredMonoBlock& block,SpectrumFrame& output); private: std::array<int16_t,1024> samples_{}; std::array<float,1024> real_{},imag_{}; std::array<uint16_t,36> smooth_{}; std::size_t count_=0; uint32_t sequence_=0,maximum_us_=0,dropped_=0; };
