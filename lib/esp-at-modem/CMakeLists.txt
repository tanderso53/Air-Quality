cmake_minimum_required(VERSION 3.22)

project(esp-at-modem)

add_subdirectory(
  ${CMAKE_CURRENT_LIST_DIR}/lib)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/builddoxygendocs.cmake)

builddoxygendocs(ESP_AT)

add_library(esp-at-modem INTERFACE)

target_sources(esp-at-modem INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/src/esp-at-modem.c
  ${CMAKE_CURRENT_LIST_DIR}/src/uart_tx.pio
  ${CMAKE_CURRENT_LIST_DIR}/src/uart_rx.pio)

pico_generate_pio_header(esp-at-modem
  ${CMAKE_CURRENT_LIST_DIR}/src/uart_tx.pio)

pico_generate_pio_header(esp-at-modem
  ${CMAKE_CURRENT_LIST_DIR}/src/uart_rx.pio)

target_include_directories(esp-at-modem INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/include)

target_link_libraries(esp-at-modem INTERFACE
  at-parse pico_stdlib hardware_pio debugmsg
  pico_multicore)
