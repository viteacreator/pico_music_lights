cmake_minimum_required(VERSION 3.13)

project(pico_music_lights_host_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

add_executable(led_logic_tests
    led_logic_tests.cpp
    ../../src/led/rgbw_conversion.cpp
    ../../src/led/led_strip.cpp
    ../../src/led/led_output_conversion.cpp
)
target_include_directories(led_logic_tests PRIVATE ../../include ../../src)
add_test(NAME led_logic_tests COMMAND led_logic_tests)
