#include <cstdint>
#if defined(PML_BUILD_OVERSIZED_FIRMWARE)
__attribute__((used, section(".rodata.pml_negative_overlap")))
const std::uint8_t kPmlIntentionalOverlap[1980000] = {1u};
extern "C" const std::uint8_t *pml_intentional_overlap_anchor() {
  return kPmlIntentionalOverlap;
}
#endif
