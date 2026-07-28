#include "audio/spectrum_resampler.hpp"
void resample_spectrum(const SpectrumFrame& s,uint16_t* d,std::size_t n){for(std::size_t i=0;i<n;++i){uint32_t sum=0,weight=0;const std::size_t a=i*32,b=(i+1)*32;for(std::size_t j=a;j<b;++j){std::size_t k=j/ n, w=1;sum+=s.bands[k]*w;weight+=w;}d[i]=sum/weight;}}
