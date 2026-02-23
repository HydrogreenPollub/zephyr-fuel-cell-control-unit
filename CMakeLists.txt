cmake_minimum_required(VERSION 3.20)

# TODO remove ========
set(VENV_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../.venv/Scripts")
set(ENV{ESPTOOL_PATH} "${VENV_PATH}/esptool.exe")

if(WIN32)
    set(ENV{PATH} "${VENV_PATH};$ENV{PATH}")
else()
    set(ENV{PATH} "${VENV_PATH}:$ENV{PATH}")
endif()

set(Python3_EXECUTABLE "${VENV_PATH}/python.exe" CACHE FILEPATH "" FORCE)
set(PYTHON_EXECUTABLE "${VENV_PATH}/python.exe" CACHE FILEPATH "" FORCE)
set(ESPTOOL "${VENV_PATH}/esptool.exe" CACHE FILEPATH "" FORCE)
# TODO remove ========

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

