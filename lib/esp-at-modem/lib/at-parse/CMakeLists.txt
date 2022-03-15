cmake_minimum_required(VERSION 3.22)

project(at-parse)

add_library(at-parse INTERFACE)

target_sources(at-parse INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/src/at-parse.c)

target_include_directories(at-parse INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/include)
