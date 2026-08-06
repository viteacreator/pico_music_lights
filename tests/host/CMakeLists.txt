cmake_minimum_required(VERSION 3.13)

project(pico_music_lights_host_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()
set(PICO_MUSIC_LIGHTS_HOST_SANITIZER "none" CACHE STRING
    "Host sanitizer: none, address, or undefined")
set_property(CACHE PICO_MUSIC_LIGHTS_HOST_SANITIZER PROPERTY STRINGS
    none address undefined)

if(NOT PICO_MUSIC_LIGHTS_HOST_SANITIZER MATCHES "^(none|address|undefined)$")
    message(FATAL_ERROR
        "PICO_MUSIC_LIGHTS_HOST_SANITIZER must be none, address, or undefined")
endif()

function(configure_host_test target)
    target_include_directories(${target} PRIVATE ../../include ../../src)

    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
        if(PICO_MUSIC_LIGHTS_HOST_SANITIZER STREQUAL "address")
            target_compile_options(${target} PRIVATE
                -fsanitize=address -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address)
        elseif(PICO_MUSIC_LIGHTS_HOST_SANITIZER STREQUAL "undefined")
            target_compile_options(${target} PRIVATE
                -fsanitize=undefined -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=undefined)
        endif()
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
    ../../src/effects/idle_lighting.cpp
    ../../src/effects/effect_rendering.cpp
    ../../src/effects/effect_scenes.cpp
)
configure_host_test(effect_engine_tests)

add_executable(device_configuration_tests
    device_configuration_tests.cpp
    ../../src/config/device_config.cpp
    ../../src/config/config_service.cpp
    ../../src/config/device_config_codec.cpp
    ../../src/config/schema_enum_mapping.cpp
    ../../src/config/runtime_adapter.cpp
    ../../src/led/led_strip.cpp
    ../../src/config/config_diagnostics.cpp
    ../../src/config/config_boot.cpp
    ../../src/config/crc32.cpp
    ../../src/storage/flash_backend.cpp
    ../../src/storage/fake_flash_backend.cpp
    ../../src/storage/persistent_record.cpp
    ../../src/storage/device_config_store.cpp
    ../../src/storage/save_coordinator.cpp
    ../../src/effects/effect_engine.cpp
    ../../src/effects/effect_rendering.cpp
    ../../src/effects/effect_scenes.cpp
)
configure_host_test(device_configuration_tests)
