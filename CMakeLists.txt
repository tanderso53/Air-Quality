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

###########################################
# Add in vendor library for BME680/BME688 #
###########################################

add_library(bme68x-sensor-api INTERFACE)

target_sources(bme68x-sensor-api INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/BME68x-Sensor-API/bme68x.c)

target_include_directories(bme68x-sensor-api INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/BME68x-Sensor-API)

#############################
# BME280 Vendor API Library #
#############################

add_library(bme280-driver INTERFACE)

target_sources(bme280-driver INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/BME280_driver/bme280.c)

target_include_directories(bme280-driver INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/BME280_driver)

################################
# Main air-quality MCU program #
################################

include(${CMAKE_CURRENT_LIST_DIR}/pm2_5-interface.cmake)

add_executable(air-quality)

# Generate Pico PIO state machine headers
pico_generate_pio_header(air-quality ${CMAKE_CURRENT_LIST_DIR}/src/ws2812.pio)

pico_generate_pio_header(air-quality ${CMAKE_CURRENT_LIST_DIR}/src/uart_tx.pio)

pico_generate_pio_header(air-quality ${CMAKE_CURRENT_LIST_DIR}/src/uart_rx.pio)

target_sources(air-quality PRIVATE
  src/air-quality.c
  src/bme680-interface.c
  src/bme280-interface.c
  src/esp-at-modem.c
  src/ws2812.pio
  src/uart_tx.pio
  src/uart_rx.pio
  )

pico_enable_stdio_usb(air-quality 1)
pico_enable_stdio_uart(air-quality 0)

target_link_libraries(air-quality pico_stdlib hardware_i2c hardware_pio
  hardware_uart bme68x-sensor-api bme280-driver pm2_5-sensor-interface
  hardware_adc)

if(AIR_QUALITY_SELF_TEST)

  target_compile_definitions(air-quality PRIVATE
    BME680_INTERFACE_SELFTEST=1)

endif()

if(AIR_QUALITY_WAIT)

  target_compile_definitions(air-quality PRIVATE
    AIR_QUALITY_WAIT_CONNECTION=1)

endif()

pico_add_extra_outputs(air-quality)
