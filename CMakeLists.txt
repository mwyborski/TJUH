cmake_minimum_required(VERSION 3.13)
project(tjuh C)

# TJUH is an INTERFACE library — it provides sources and include paths to
# consuming projects but does not produce a standalone build artifact.
# To build, add this directory from a Pico SDK project:
#
#   add_subdirectory(lib/tjuh)
#   target_link_libraries(my_app tjuh tinyusb_host tinyusb_board)
#
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    message(STATUS "")
    message(STATUS "TJUH is an INTERFACE library and has no standalone build target.")
    message(STATUS "Add it as a subdirectory from a Pico SDK project. Example:")
    message(STATUS "")
    message(STATUS "  add_subdirectory(lib/tjuh)")
    message(STATUS "  target_link_libraries(my_app tjuh tinyusb_host tinyusb_board)")
    message(STATUS "")
endif()

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
# Set TJUH_USE_REFERENCE_CONFIG=ON (default) to include cfg/ in the search path.
# Set it to OFF if the application provides its own tusb_config.h.
option(TJUH_USE_REFERENCE_CONFIG "Use the reference tusb_config.h shipped with TJUH" ON)

if(TJUH_USE_REFERENCE_CONFIG)
    target_include_directories(tjuh INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/cfg
    )
endif()
