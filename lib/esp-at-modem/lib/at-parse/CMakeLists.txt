cmake_minimum_required(VERSION 3.22)

project(at-parse)

option(AT_PARSE_BUILD_TESTS
  "Build tests for AT Response Parsing library"
  OFF)
option(AT_PARSE_BUILD_DOCS
  "Build documentation for AT Response Parsing Library"
  OFF)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/builddoxygendocs.cmake)

# Build docs if AT_PARSE_BUILD_DOCS is ON
builddoxygendocs(AT_PARSE)

add_library(at-parse INTERFACE)

target_sources(at-parse INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/src/at-parse.c)

target_include_directories(at-parse INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/include)

##################
# TESTING MODULE #
##################

if (AT_PARSE_BUILD_TESTS)

  enable_testing()

  add_executable(at-parse-test-suite
    ${CMAKE_CURRENT_LIST_DIR}/tests/tests.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/munit/munit.c)

  target_link_libraries(at-parse-test-suite PRIVATE
    at-parse)

  target_include_directories(at-parse-test-suite PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/lib/munit)

  target_compile_options(at-parse INTERFACE -Wall -g)

  target_compile_options(at-parse-test-suite PRIVATE
    -Wall -g)

  add_test(NAME at-parse-tests
    COMMAND $<TARGET_FILE:at-parse-test-suite>)

endif()
