cmake_minimum_required(VERSION 3.22)

project(esp-at-modem)

option(ESP_AT_MULTICORE
  "Include multi-core capability in interface"
  OFF)

add_subdirectory(
  ${CMAKE_CURRENT_LIST_DIR}/lib)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/builddoxygendocs.cmake)

builddoxygendocs(ESP_AT)

add_library(esp-at-modem INTERFACE)

target_sources(esp-at-modem INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/src/esp-at-modem.c)

target_include_directories(esp-at-modem INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/include)

target_link_libraries(esp-at-modem INTERFACE
  at-parse uart-pio pico_stdlib hardware_pio debugmsg)

if(ESP_AT_MULTICORE)
  target_compile_definitions(esp-at-modem INTERFACE
    ESP_AT_MULTICORE_ENABLED)

  target_link_libraries(esp-at-modem INTERFACE
    pico_multicore)
endif()
