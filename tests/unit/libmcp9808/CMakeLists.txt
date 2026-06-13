cmake_minimum_required(VERSION 3.10)
project(test-libmcp9808 CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(GTest REQUIRED)

include(GoogleTest)

add_executable(test_mcp9808 test_mcp9808.cpp)
target_link_libraries(test_mcp9808
    PRIVATE mcp9808 GTest::GTest GTest::Main GTest::gmock pthread
)
gtest_discover_tests(test_mcp9808
    XML_OUTPUT_DIR ${CMAKE_BINARY_DIR}/test-results
)
