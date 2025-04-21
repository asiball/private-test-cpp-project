cmake_minimum_required(VERSION 3.10)
project(test-spi-driver CXX)

find_package(GTest REQUIRED)

add_executable(test_spi_driver test_spi_driver.cpp)
target_link_libraries(test_spi_driver
    PRIVATE spi_driver GTest::GTest GTest::Main
)
target_include_directories(test_spi_driver PRIVATE
    ${CMAKE_SOURCE_DIR}/driver/include
)

include(GoogleTest)
gtest_discover_tests(test_spi_driver
    XML_OUTPUT_DIR ${CMAKE_BINARY_DIR}/test-results
)
