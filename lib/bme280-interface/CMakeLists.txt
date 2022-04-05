cmake_minimum_required(VERSION 3.18)

project(bme280-interface)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib)

add_library(${PROJECT_NAME} INTERFACE)

target_sources(${PROJECT_NAME} INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/src/bme280-interface.c)

target_include_directories(${PROJECT_NAME} INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/include)

target_link_libraries(${PROJECT_NAME} INTERFACE
  bme280-driver)
