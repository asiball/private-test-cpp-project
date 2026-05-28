cmake_minimum_required(VERSION 3.10)
project(test-libsensor CXX)

find_package(GTest REQUIRED)

add_executable(test_sensor test_sensor.cpp)
target_link_libraries(test_sensor
    PRIVATE sensor GTest::GTest GTest::Main GTest::gmock pthread
)
target_include_directories(test_sensor PRIVATE
    ${CMAKE_SOURCE_DIR}/libsensor/include
)

include(GoogleTest)
gtest_discover_tests(test_sensor
    XML_OUTPUT_DIR ${CMAKE_BINARY_DIR}/test-results
)
