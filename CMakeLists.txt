cmake_minimum_required(VERSION 3.13)

# Detect standalone build vs. inclusion as a submodule
set(TJUH_STANDALONE FALSE)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(TJUH_STANDALONE TRUE)
endif()

# When building standalone, initialize the Pico SDK and build examples
if(TJUH_STANDALONE)
    include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

    project(tjuh C CXX ASM)
    set(CMAKE_C_STANDARD 11)
    set(CMAKE_CXX_STANDARD 17)

    pico_sdk_init()
endif()

# ---------------------------------------------------------------------- #
#  TJUH library (INTERFACE — always available)                            #
# ---------------------------------------------------------------------- #

add_library(tjuh INTERFACE)

target_sources(tjuh INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/src/tjuh.c
    ${CMAKE_CURRENT_LIST_DIR}/src/tjuh_parse.c
)

target_include_directories(tjuh INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/src
)

# Optionally expose the reference tusb_config.h.
# Set TJUH_USE_REFERENCE_CONFIG=OFF if the application provides its own.
option(TJUH_USE_REFERENCE_CONFIG "Use the reference tusb_config.h shipped with TJUH" ON)

if(TJUH_USE_REFERENCE_CONFIG)
    target_include_directories(tjuh INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/cfg
    )
endif()

# ---------------------------------------------------------------------- #
#  Examples (standalone build only)                                       #
# ---------------------------------------------------------------------- #

if(TJUH_STANDALONE)
    option(TJUH_BUILD_EXAMPLES "Build TJUH examples" ON)
    option(TJUH_EXAMPLE_ENABLE_PIN_OUTPUT "Enable PWM and GPIO pin outputs in examples" ON)

    if(TJUH_BUILD_EXAMPLES)
        add_subdirectory(examples/pwm_output)
    endif()
endif()
