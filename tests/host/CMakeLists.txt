cmake_minimum_required(VERSION 3.13)

project(pico_music_lights_host_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

function(configure_host_test target)
    target_include_directories(${target} PRIVATE ../../include ../../src)

    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME ${target} COMMAND ${target})
endfunction()

add_executable(led_logic_tests
    led_logic_tests.cpp
    ../../src/led/rgbw_conversion.cpp
    ../../src/led/led_strip.cpp
    ../../src/led/led_output_conversion.cpp
    ../../src/led/led_frame_state.cpp
)
configure_host_test(led_logic_tests)

add_executable(audio_processing_tests
    audio_processing_tests.cpp
    ../../src/audio/audio_processing.cpp
)
configure_host_test(audio_processing_tests)

add_executable(spectrum_analysis_tests
    spectrum_analysis_tests.cpp
    ../../src/audio/spectrum_analyzer.cpp
    ../../src/audio/spectrum_q15_backend.cpp
    ../../src/audio/spectrum_resampler.cpp
)
configure_host_test(spectrum_analysis_tests)

add_executable(diagnostic_renderer_tests
    diagnostic_renderer_tests.cpp
    ../../src/led/diagnostic_rendering.cpp
    ../../src/audio/spectrum_resampler.cpp
)
configure_host_test(diagnostic_renderer_tests)

add_executable(effect_engine_tests
    effect_engine_tests.cpp
    ../../src/effects/effect_engine.cpp
    ../../src/effects/effect_rendering.cpp
)
configure_host_test(effect_engine_tests)
