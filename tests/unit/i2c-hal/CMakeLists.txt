cmake_minimum_required(VERSION 3.10)
project(test-i2c-hal CXX)

find_package(GTest REQUIRED)

add_executable(test_i2c_driver test_i2c_driver.cpp)
target_link_libraries(test_i2c_driver
    PRIVATE i2chal GTest::GTest GTest::Main
)
target_include_directories(test_i2c_driver PRIVATE
    ${CMAKE_SOURCE_DIR}/i2c-hal/include
)

include(GoogleTest)
gtest_discover_tests(test_i2c_driver
    XML_OUTPUT_DIR ${CMAKE_BINARY_DIR}/test-results
)
