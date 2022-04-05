cmake_minimum_required(VERSION 3.18)

#############################
# BME280 Vendor API Library #
#############################

add_library(bme280-driver INTERFACE)

target_sources(bme280-driver INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/BME280_driver/bme280.c)

target_include_directories(bme280-driver INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/BME280_driver)
