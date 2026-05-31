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

# ── ADS1115 テスト（テストソースがある場合のみ）──────────────
# tests は CI でスタンドアロンにビルドされ、ads1115 はインストール済みライブラリ
# として名前解決される（test_sensor が sensor をリンクするのと同じ方式）。
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_ads1115.cpp)
    add_executable(test_ads1115 test_ads1115.cpp)
    target_link_libraries(test_ads1115
        PRIVATE ads1115 GTest::GTest GTest::Main GTest::gmock pthread
    )
    target_include_directories(test_ads1115 PRIVATE
        ${CMAKE_SOURCE_DIR}/libsensor/include
        ${CMAKE_SOURCE_DIR}/i2c-hal/include
    )
    gtest_discover_tests(test_ads1115
        XML_OUTPUT_DIR ${CMAKE_BINARY_DIR}/test-results
    )
endif()
