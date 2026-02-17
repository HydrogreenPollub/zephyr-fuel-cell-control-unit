cmake_minimum_required(VERSION 3.20)
set(BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR} CACHE PATH "Root directory for custom boards")
set(BOARD fccu_v2/esp32s3/procpu CACHE STRING "Zephyr target board")

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(app LANGUAGES C)

target_sources(app PRIVATE
        src/main.c
        src/fccu_v2.c
        peripherals/can.c
        peripherals/gpio.c
        peripherals/adc.c
        peripherals/pwm.c
        peripherals/ads1015.c
        peripherals/counter.c
)

target_include_directories(app PRIVATE
        peripherals
        can-definitions
        src
)