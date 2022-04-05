cmake_minimum_required(VERSION 3.18)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib)

###################################################
# Pico Interface Driver for PMS 5003 PM2.5 Sensor #
###################################################

add_library(pm2_5-sensor-interface INTERFACE)

target_sources(pm2_5-sensor-interface INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/src/pm2_5-interface.c)

target_include_directories(pm2_5-sensor-interface INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/include)

target_link_libraries(pm2_5-sensor-interface INTERFACE
  pm2_5-sensor-api
  pico_stdlib hardware_uart)
