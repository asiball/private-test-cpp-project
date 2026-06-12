cmake_minimum_required(VERSION 3.10)
project(test-integration CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(GTest REQUIRED)

include(GoogleTest)

add_executable(test_mcp3008 test_mcp3008.cpp)
target_link_libraries(test_mcp3008
    PRIVATE sensor GTest::GTest GTest::Main pthread
)
gtest_discover_tests(test_mcp3008
    XML_OUTPUT_DIR ${CMAKE_BINARY_DIR}/test-results
)

# 追加の結合テスト（libadxl345 / ADS1115 / gpio）。対応ライブラリを install 済みの
# 名前でリンクする。実機が無い CI 環境では各テストの SetUp で GTEST_SKIP される。
add_executable(test_adxl345 test_adxl345.cpp)
target_link_libraries(test_adxl345
    PRIVATE adxl345 GTest::GTest GTest::Main pthread
)
gtest_discover_tests(test_adxl345
    XML_OUTPUT_DIR ${CMAKE_BINARY_DIR}/test-results
)

add_executable(test_ads1115 test_ads1115.cpp)
target_link_libraries(test_ads1115
    PRIVATE ads1115 GTest::GTest GTest::Main pthread
)
gtest_discover_tests(test_ads1115
    XML_OUTPUT_DIR ${CMAKE_BINARY_DIR}/test-results
)

add_executable(test_gpio test_gpio.cpp)
target_link_libraries(test_gpio
    PRIVATE gpio GTest::GTest GTest::Main pthread
)
gtest_discover_tests(test_gpio
    XML_OUTPUT_DIR ${CMAKE_BINARY_DIR}/test-results
)
