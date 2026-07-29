#include "led/diagnostic_rendering.hpp"

#include <array>
#include <cstdio>

namespace {

bool is_off(const RgbwColor& color) {
    return color.red == 0 && color.green == 0 && color.blue == 0 &&
           color.white == 0;
}

bool test_spectrum_mapping_and_rgbw() {
    SpectrumFrame spectrum{};
    spectrum.bands[0] = 65535;
    spectrum.bands[1] = 65535;

    std::array<RgbwColor, 19> pixels{};
    render_spectrum_32_to_16({pixels.data(), pixels.size()}, spectrum);

    if (is_off(pixels[0]) || is_off(pixels[1]) || !is_off(pixels[2]) ||
        pixels[0].white == 0) {
        return false;
    }

    render_spectrum_32_to_16({pixels.data(), pixels.size()}, SpectrumFrame{});
    for (const RgbwColor& pixel : pixels) {
        if (!is_off(pixel)) {
            return false;
        }
    }

    return true;
}

bool test_symmetric_and_macro_zone_boundaries() {
    SpectrumFrame spectrum{};
    spectrum.bands[0] = 65535;
    spectrum.bands[31] = 32768;
    std::array<RgbwColor, 11> zones{};
    render_symmetric_frequency_zones({zones.data(), zones.size()}, spectrum);

    if (zones[0].red != zones[10].red || zones[1].red != zones[9].red ||
        zones[0].blue != zones[10].blue) {
        return false;
    }

    spectrum.bass = 65535;
    spectrum.mid = 32768;
    spectrum.high = 16384;
    std::array<RgbwColor, 10> macro{};
    render_bass_mid_high_zones({macro.data(), macro.size()}, spectrum);
    return macro[0].red == 255 && macro[4].green == 128 &&
           macro[7].blue == 64;
}

bool test_stereo_geometry() {
    std::array<RgbwColor, 8> even{};
    render_stereo_center_out({even.data(), even.size()}, 65535, 32768,
                             {255, 0, 0, 0}, {0, 0, 255, 0});
    if (even[3].red != 255 || even[0].red != 255 || even[4].blue != 255 ||
        even[5].blue != 255 || !is_off(even[6])) {
        return false;
    }

    std::array<RgbwColor, 5> odd{};
    render_stereo_center_out({odd.data(), odd.size()}, 65535, 65535,
                             {255, 0, 0, 0}, {0, 0, 255, 0});
    if (odd[0].red != 255 || odd[2].red != 255 || odd[3].blue != 255 ||
        odd[4].blue != 255) {
        return false;
    }

    std::array<RgbwColor, 3> left_only{};
    render_stereo_center_out({left_only.data(), left_only.size()}, 65535, 0,
                             {255, 0, 0, 0}, {0, 0, 255, 0});
    if (left_only[0].red != 255 || left_only[1].red != 255 ||
        !is_off(left_only[2])) {
        return false;
    }

    std::array<RgbwColor, 3> right_only{};
    render_stereo_center_out({right_only.data(), right_only.size()}, 0, 65535,
                             {255, 0, 0, 0}, {0, 0, 255, 0});
    return is_off(right_only[0]) && is_off(right_only[1]) &&
           right_only[2].blue == 255;
}

bool test_vu_clear_short_and_clamp() {
    std::array<RgbwColor, 2> pixels{{{255, 255, 255, 255},
                                      {255, 255, 255, 255}}};
    render_full_vu({pixels.data(), pixels.size()}, 0, {255, 255, 255, 255});
    if (!is_off(pixels[0]) || !is_off(pixels[1])) {
        return false;
    }

    render_full_vu({pixels.data(), pixels.size()}, 65535, {255, 255, 255, 255});
    if (pixels[0].red != 255 || pixels[0].white != 255 ||
        pixels[1].green != 255 || pixels[1].blue != 255) {
        return false;
    }

    return !diagnostic_frame_should_start(false, false, 1000, 0, 1) &&
           !diagnostic_frame_should_start(true, true, 1000, 0, 1) &&
           diagnostic_frame_should_start(true, false, 1000, 0, 1);
}

struct NamedTest {
    const char* name;
    bool (*function)();
};

}  // namespace

int main() {
    constexpr std::array<NamedTest, 4> kTests{{
        {"spectrum mapping and RGBW", test_spectrum_mapping_and_rgbw},
        {"symmetric and macro zones", test_symmetric_and_macro_zone_boundaries},
        {"stereo geometry", test_stereo_geometry},
        {"VU clear, short span, and clamp", test_vu_clear_short_and_clamp},
    }};

    for (const NamedTest& test : kTests) {
        if (!test.function()) {
            std::fprintf(stderr, "diagnostic renderer test failed: %s\n", test.name);
            return 1;
        }
    }

    return 0;
}
