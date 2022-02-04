cmake_minimum_required(VERSION 3.22)

#########################################
# API library for PMS 5003 PM2.5 Sensor #
#########################################

add_library(pm2_5-sensor-api INTERFACE)

target_sources(pm2_5-sensor-api INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/src/pm2_5.c)

target_include_directories(pm2_5-sensor-api INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/include)
