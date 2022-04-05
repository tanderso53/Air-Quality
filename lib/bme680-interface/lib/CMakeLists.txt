cmake_minimum_required(VERSION 3.18)

###########################################
# Add in vendor library for BME680/BME688 #
###########################################

add_library(bme68x-sensor-api INTERFACE)

target_sources(bme68x-sensor-api INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/BME68x-Sensor-API/bme68x.c)

target_include_directories(bme68x-sensor-api INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/BME68x-Sensor-API)
