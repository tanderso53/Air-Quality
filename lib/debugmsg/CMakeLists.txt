cmake_minimum_required(VERSION 3.22)

project(debugmsg)

add_library(debugmsg INTERFACE)

target_include_directories(debugmsg INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/include)
