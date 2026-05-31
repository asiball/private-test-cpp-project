cmake_minimum_required(VERSION 3.10)
project(test-libadxl345 CXX)

find_package(GTest REQUIRED)

add_executable(test_adxl345 test_adxl345.cpp)
target_link_libraries(test_adxl345
    PRIVATE adxl345 GTest::GTest GTest::Main GTest::gmock pthread
)
target_include_directories(test_adxl345 PRIVATE
    ${CMAKE_SOURCE_DIR}/libadxl345/include
)

include(GoogleTest)
gtest_discover_tests(test_adxl345
    XML_OUTPUT_DIR ${CMAKE_BINARY_DIR}/test-results
)
