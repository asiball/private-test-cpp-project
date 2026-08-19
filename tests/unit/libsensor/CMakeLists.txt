cmake_minimum_required(VERSION 3.10)
project(test-libsensor CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
#
# 統合経路（トップから add_subdirectory された場合）では、このファイルが処理される
# 時点で libsensor/CMakeLists.txt が既に走っているため、実際に CMake target として
# "sensor" が定義されていれば TARGET sensor が真になる（スタンドアロン経路では
# "sensor" はリンカへ渡すだけの生の名前でしかなく target は定義されないため偽になる）。
# この違いを使って経路を判定し、統合経路では ads1115 target が無い（i2c-hal を
# 外した構成）場合に test_ads1115 をスキップする。スタンドアロン経路は従来どおり
# 常にビルドを試みる（インストール済みライブラリを名前解決）。
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_ads1115.cpp)
    set(_build_test_ads1115 TRUE)
    if(TARGET sensor AND NOT TARGET ads1115)
        # 統合経路かつ ads1115 target が無い（i2c-hal 未ビルド）→ スキップ
        set(_build_test_ads1115 FALSE)
    endif()

    if(_build_test_ads1115)
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
    else()
        message(STATUS "test-libsensor: ads1115 target not found; skipping test_ads1115")
    endif()
endif()
