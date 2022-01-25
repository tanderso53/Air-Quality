cmake_minimum_required(VERSION 3.22)

# Add environment variables
include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)
set(PICO_TOOLCHAIN_PATH $ENV{PICO_TOOLCHAIN_PATH})


# Change the below information to match the project
# you wish to build
project(air-quality)
pico_sdk_init()

add_executable(air-quality)

pico_generate_pio_header(air-quality ${CMAKE_CURRENT_LIST_DIR}/src/ws2812.pio)

target_sources(air-quality PRIVATE
	src/air-quality.c
	src/bme680-interface.c
	src/ws2812.pio
	BME68x-Sensor-API/bme68x.c
	)

pico_enable_stdio_usb(air-quality 1)
pico_enable_stdio_uart(air-quality 0)

target_include_directories(air-quality BEFORE PRIVATE
	BME68x-Sensor-API)

target_link_libraries(air-quality pico_stdlib hardware_i2c hardware_pio)

#add_compile_definitions(BME680_INTERFACE_SELFTEST=1)

pico_add_extra_outputs(air-quality)
