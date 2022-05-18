cmake_minimum_required(VERSION 3.22)

project(uart-pio)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/builddoxygendocs.cmake)

builddoxygendocs(UART_PIO)

add_library(${PROJECT_NAME} INTERFACE)

target_sources(${PROJECT_NAME} INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/src/uart_pio.c
  ${CMAKE_CURRENT_LIST_DIR}/src/uart_tx.pio
  ${CMAKE_CURRENT_LIST_DIR}/src/uart_rx.pio)

pico_generate_pio_header(${PROJECT_NAME}
  ${CMAKE_CURRENT_LIST_DIR}/src/uart_tx.pio)

pico_generate_pio_header(${PROJECT_NAME}
  ${CMAKE_CURRENT_LIST_DIR}/src/uart_rx.pio)

target_include_directories(${PROJECT_NAME} INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/include)

target_link_libraries(${PROJECT_NAME} INTERFACE
  pico_stdlib hardware_pio)
