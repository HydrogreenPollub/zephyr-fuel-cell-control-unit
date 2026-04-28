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

target_compile_options(app PRIVATE -mno-text-section-literals)

target_sources(app PRIVATE
        src/main.c
        src/fccu.c
        src/fccu_analog.c
        src/fccu_fan.c
        src/fccu_digital.c
        src/fccu_flow.c
        src/fccu_log.c
        src/fccu_shell.c
        peripherals/can.c
        peripherals/gpio.c
        peripherals/adc.c
        peripherals/pwm.c
        peripherals/counter.c
        peripherals/status_led.c
        src/ads1015.c
        can-definitions/candef.c
)

# fccu_analog.c uses float literals (sensor_value_to_float, 10.0f, etc).
# On ESP32-S3, the IBUS (0x42000000) and DBUS (0x3C000000) are separate virtual
# address spaces. L32R is a data load and goes via DBUS; it can only reach
# literal pools that the DBUS has mapped. Literal pools in IROM (0x42xxxxxx)
# are unreachable by DBUS when the I-Cache line is cold. Moving fccu_analog to
# IRAM keeps both code and literal pools in IRAM, which DBUS can access.
zephyr_code_relocate(FILES src/fccu_analog.c LOCATION SRAM)

target_include_directories(app PRIVATE
        peripherals
        can-definitions
        src
)
