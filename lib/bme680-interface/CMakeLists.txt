cmake_minimum_required(VERSION 3.18)

project(bme680-interface)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib)

add_library(${PROJECT_NAME} INTERFACE)

target_sources(${PROJECT_NAME} INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/src/bme680-interface.c)

target_include_directories(${PROJECT_NAME} INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/include)

target_link_libraries(${PROJECT_NAME} INTERFACE
  bme68x-sensor-api)
