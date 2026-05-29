cmake_minimum_required(VERSION 3.10)
project(test-integration CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(GTest REQUIRED)

add_executable(test_mcp3008 test_mcp3008.cpp)
target_link_libraries(test_mcp3008
    PRIVATE sensor GTest::GTest GTest::Main pthread
)

include(GoogleTest)
gtest_discover_tests(test_mcp3008
    XML_OUTPUT_DIR ${CMAKE_BINARY_DIR}/test-results
)
