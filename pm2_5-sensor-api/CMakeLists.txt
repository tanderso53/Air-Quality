cmake_minimum_required(VERSION 3.22)

#########################################
# API library for PMS 5003 PM2.5 Sensor #
#########################################

add_library(pm2_5-sensor-api INTERFACE)

target_sources(pm2_5-sensor-api INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/src/pm2_5.c)

target_include_directories(pm2_5-sensor-api INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/include)

#########
# Tests #
#########

if(DEFINED PM2_5_BUILD_TESTS)

  enable_testing()

  add_executable(pm2_5-sensor-test
    ${CMAKE_CURRENT_LIST_DIR}/tests/pm2_5-api-tests.c)

  target_link_libraries(pm2_5-sensor-test PRIVATE
    pm2_5-sensor-api)

  add_test(NAME pm2_5-sensor-test-suite
    COMMAND $<TARGET_FILE:pm2_5-sensor-test>)

endif()
