cmake_minimum_required(VERSION 3.22)

# Add environment variables
include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)
set(PICO_TOOLCHAIN_PATH $ENV{PICO_TOOLCHAIN_PATH})


# Change the below information to match the project
# you wish to build
project(air-quality)
pico_sdk_init()

add_executable(air-quality
	src/air-quality.c
	src/bme680-interface.c
	BME68x-Sensor-API/bme68x.c
	)

target_include_directories(air-quality BEFORE PRIVATE
	BME68x-Sensor-API)

target_link_libraries(air-quality pico_stdlib hardware_i2c)

pico_add_extra_outputs(air-quality)
