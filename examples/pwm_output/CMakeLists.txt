cmake_minimum_required(VERSION 3.13)

# When built standalone (not from the TJUH root), initialize SDK and pull in TJUH
if(NOT TARGET tjuh)
    include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

    project(tjuh_pwm_example C CXX ASM)
    set(CMAKE_C_STANDARD 11)
    set(CMAKE_CXX_STANDARD 17)

    pico_sdk_init()

    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../.. tjuh_lib)

    option(TJUH_EXAMPLE_ENABLE_PIN_OUTPUT "Enable PWM and GPIO pin outputs" ON)
endif()

add_executable(tjuh_pwm_example
    main.c
)

target_link_libraries(tjuh_pwm_example
    pico_stdlib
    tinyusb_host
    tinyusb_board
    tjuh
)

if(TJUH_EXAMPLE_ENABLE_PIN_OUTPUT)
    target_link_libraries(tjuh_pwm_example hardware_pwm)
    target_compile_definitions(tjuh_pwm_example PRIVATE TJUH_EXAMPLE_ENABLE_PIN_OUTPUT=1)
else()
    target_compile_definitions(tjuh_pwm_example PRIVATE TJUH_EXAMPLE_ENABLE_PIN_OUTPUT=0)
endif()

# USB port is occupied by TinyUSB host mode — use UART for serial output
pico_enable_stdio_usb(tjuh_pwm_example 0)
pico_enable_stdio_uart(tjuh_pwm_example 1)

pico_add_extra_outputs(tjuh_pwm_example)
