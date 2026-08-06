# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.0)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(pico_music_lights C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(pico_music_lights
        src/audio/audio_app.cpp
        src/config/device_config.cpp
        src/config/config_service.cpp
        src/config/device_config_codec.cpp
        src/config/crc32.cpp
        src/storage/flash_backend.cpp
        src/storage/persistent_record.cpp
        src/storage/device_config_store.cpp
        src/storage/save_coordinator.cpp
        src/storage/rp2040_flash_backend.cpp
        src/audio/audio_capture.cpp
        src/audio/audio_processing.cpp
        src/audio/spectrum_analyzer.cpp
        src/audio/spectrum_q15_backend.cpp
        src/audio/spectrum_resampler.cpp
        src/effects/effect_engine.cpp
        src/effects/idle_lighting.cpp
        src/effects/effect_rendering.cpp
        src/effects/effect_scenes.cpp
        src/validation/led_public_api_compile.cpp
        src/led/rgbw_conversion.cpp
        src/led/led_strip.cpp
        src/led/led_output_conversion.cpp
        src/led/led_frame_state.cpp
        src/led/led_output_manager.cpp
        src/led/sk6812_rgbw_driver.cpp
        src/led/diagnostic_rendering.cpp
        src/led/diagnostic_renderer.cpp
        )

pico_set_program_name(pico_music_lights "pico_music_lights")
pico_set_program_version(pico_music_lights "0.1")

# Generate PIO header
pico_generate_pio_header(pico_music_lights ${CMAKE_CURRENT_LIST_DIR}/pio/sk6812_rgbw.pio)

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(pico_music_lights 0)
pico_enable_stdio_usb(pico_music_lights 1)

# Add the standard library to the build
target_link_libraries(pico_music_lights
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(pico_music_lights PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${CMAKE_CURRENT_LIST_DIR}/src
)

# Add any user requested libraries
target_link_libraries(pico_music_lights 
        hardware_dma
        hardware_pio
        hardware_adc
        hardware_flash
        
        )

# Bound USB CDC backpressure so one telemetry report cannot consume a material
# fraction of the approximately 8 ms audio-block processing budget.
target_compile_definitions(pico_music_lights PRIVATE
        PICO_STDIO_USB_STDOUT_TIMEOUT_US=1000
        CFG_TUD_CDC_TX_BUFSIZE=512
        )

pico_add_extra_outputs(pico_music_lights)

