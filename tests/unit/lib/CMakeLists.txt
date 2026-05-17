cmake_minimum_required(VERSION 3.10)
project(test-device CXX)

find_package(GTest REQUIRED)

add_executable(test_device test_device.cpp)
target_link_libraries(test_device
    PRIVATE device GTest::GTest GTest::Main GTest::gmock pthread
)
target_include_directories(test_device PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/include
)

include(GoogleTest)
gtest_discover_tests(test_device
    XML_OUTPUT_DIR ${CMAKE_BINARY_DIR}/test-results
)
