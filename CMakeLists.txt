cmake_minimum_required(VERSION 3.22)

# Add environment variables
include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)
set(PICO_TOOLCHAIN_PATH $ENV{PICO_TOOLCHAIN_PATH})


# Change the below information to match the project
# you wish to build
project(air-quality)
pico_sdk_init()

# Add options
option(AIR_QUALITY_SELF_TEST "Perform self test sequence at start"
  OFF)

option(AIR_QUALITY_WAIT "Wait on connection for application to begin"
  OFF)

option(AIR_QUALITY_LOG_LEVEL_DEBUG "Enable debug messages to stdout"
  OFF)

option(AIR_QUALITY_TARGET_WING "Compile for the Air Quality Wing variant"
  ON)

set(ESP_AT_MULTICORE ON)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib)

################################
# Main air-quality MCU program #
################################

add_executable(air-quality)

# Generate Pico PIO state machine headers
pico_generate_pio_header(air-quality ${CMAKE_CURRENT_LIST_DIR}/src/ws2812.pio)

target_sources(air-quality PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/src/air-quality.c
  ${CMAKE_CURRENT_LIST_DIR}/src/aq-error-state.c
  ${CMAKE_CURRENT_LIST_DIR}/src/aq-stdio.c
  ${CMAKE_CURRENT_LIST_DIR}/src/ws2812.pio
)

pico_enable_stdio_usb(air-quality 1)
pico_enable_stdio_uart(air-quality 0)

target_link_libraries(air-quality pico_stdlib hardware_i2c hardware_pio
  hardware_uart bme680-interface pm2_5-sensor-interface
  hardware_adc esp-at-modem debugmsg pico_multicore pico_util)

#########################
# Process CMAKE options #
#########################

if(AIR_QUALITY_SELF_TEST)

  target_compile_definitions(air-quality PRIVATE
    BME680_INTERFACE_SELFTEST=1)

endif()

if(AIR_QUALITY_WAIT)

  target_compile_definitions(air-quality PRIVATE
    AIR_QUALITY_WAIT_CONNECTION=1)

endif()

# Produce debug messages during runtime
if (AIR_QUALITY_LOG_LEVEL_DEBUG)

  target_compile_definitions(air-quality PRIVATE
    AIR_QUALITY_LOG_DEBUG=1)

endif()

# Compile for wing
if (AIR_QUALITY_TARGET_WING)

  target_compile_definitions(air-quality PRIVATE
    AIR_QUALITY_COMPILE_TARGET_WING=1
    AIR_QUALITY_WIFI_GPIO_EN_PIN=7
    PM2_5_INTERFACE_GPIO_EN_PIN=25)

endif()

# Need this to get our .uf2
pico_add_extra_outputs(air-quality)
