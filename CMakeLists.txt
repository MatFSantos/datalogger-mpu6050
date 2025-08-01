# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PICO_BOARD pico_w CACHE STRING "Board type")
include(pico_sdk_import.cmake)

project(main C CXX ASM)
pico_sdk_init()

add_subdirectory(lib/FatFs_SPI)

add_executable(${PROJECT_NAME} main.c hw_config.c)

pico_set_program_name(${PROJECT_NAME} "Datalogger MPU6050")
pico_set_program_version(${PROJECT_NAME} "0.1")

pico_enable_stdio_uart(${PROJECT_NAME} 0)
pico_enable_stdio_usb(${PROJECT_NAME} 1)

target_link_libraries(${PROJECT_NAME}
    pico_stdlib
    hardware_gpio
    FatFs_SPI
    hardware_clocks
    hardware_i2c
    hardware_pwm
)

target_sources(${PROJECT_NAME} PRIVATE
    lib/ssd1306.c
    lib/led.c
    lib/pwm.c
    lib/push_button.c
)

pico_add_extra_outputs(${PROJECT_NAME})