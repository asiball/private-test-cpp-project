cmake_minimum_required(VERSION 3.10)
project(test-gpio CXX)

find_package(GTest REQUIRED)

add_executable(test_gpio_line test_gpio_line.cpp)
target_link_libraries(test_gpio_line
    PRIVATE gpio GTest::GTest GTest::Main
)
target_include_directories(test_gpio_line PRIVATE
    ${CMAKE_SOURCE_DIR}/gpio/include
)

include(GoogleTest)
gtest_discover_tests(test_gpio_line
    XML_OUTPUT_DIR ${CMAKE_BINARY_DIR}/test-results
)
