cmake_minimum_required(VERSION 3.10)
project(embedded-device-suite VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── サニタイザーオプション ────────────────────────────────────
# 使い方:
#   cmake -DENABLE_SANITIZERS=asan  → AddressSanitizer + UBSan
#   cmake -DENABLE_SANITIZERS=tsan  → ThreadSanitizer
#   cmake -DENABLE_SANITIZERS=none  → なし（デフォルト）
#
# ASAN と TSAN は同時使用不可。別々のビルドディレクトリで実行すること。
set(ENABLE_SANITIZERS "none" CACHE STRING "Enable sanitizers: none | asan | tsan")
set_property(CACHE ENABLE_SANITIZERS PROPERTY STRINGS none asan tsan)

if(ENABLE_SANITIZERS STREQUAL "asan")
    message(STATUS "Sanitizers: AddressSanitizer + UBSan enabled")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
elseif(ENABLE_SANITIZERS STREQUAL "tsan")
    message(STATUS "Sanitizers: ThreadSanitizer enabled")
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
elseif(NOT ENABLE_SANITIZERS STREQUAL "none")
    message(FATAL_ERROR "ENABLE_SANITIZERS must be: none | asan | tsan")
endif()

# ── バージョン情報を git tag から自動取得 ────────────────────
find_package(Git QUIET)

function(get_git_tag pattern out_var fallback)
    if(GIT_FOUND)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} describe --tags --match "${pattern}" --abbrev=0
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE _tag
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(_tag)
            set(${out_var} "${_tag}" PARENT_SCOPE)
            return()
        endif()
    endif()
    set(${out_var} "${fallback}" PARENT_SCOPE)
endfunction()

get_git_tag("spi-hal/v*"   EDS_VERSION_SPIHAL    "spi-hal/v0.0.0-unknown")
get_git_tag("libsensor/v*" EDS_VERSION_LIBSENSOR "libsensor/v0.0.0-unknown")
get_git_tag("cli/v*"       EDS_VERSION_CLI       "cli/v0.0.0-unknown")

if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE EDS_GIT_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
else()
    set(EDS_GIT_COMMIT "unknown")
endif()

string(TIMESTAMP EDS_BUILD_DATE "%Y-%m-%d")

message(STATUS "Version: spi-hal=${EDS_VERSION_SPIHAL} libsensor=${EDS_VERSION_LIBSENSOR} cli=${EDS_VERSION_CLI}")
message(STATUS "Commit:  ${EDS_GIT_COMMIT}  Date: ${EDS_BUILD_DATE}")

# version.hpp.in → ${CMAKE_BINARY_DIR}/generated/version.hpp を生成（spi-hal がある場合のみ）。
# 生成物はソースツリーではなくビルドツリーへ書き出す。これにより spi-hal の
# install(DIRECTORY include/) が生成済み version.hpp や version.hpp.in を巻き込んで
# インストールするのを防ぐ。消費側（cli）は generated/ を include パスに追加する。
if(EXISTS ${CMAKE_SOURCE_DIR}/spi-hal/include/version.hpp.in)
    configure_file(
        ${CMAKE_SOURCE_DIR}/spi-hal/include/version.hpp.in
        ${CMAKE_BINARY_DIR}/generated/version.hpp
        @ONLY
    )
endif()

# ── find_package 対応ヘルパ（issue #47）──────────────────────────────────
# install(EXPORT) + <pkg>Config.cmake 生成を共通化する eds_install_package() を定義。
# モノレポビルド（このトップレベル経由）でのみ読み込まれ、各コンポーネントは
# COMMAND ガードで呼ぶため、単体ビルドでは従来どおり素の install になる。
include(${CMAKE_SOURCE_DIR}/cmake/EdsPackage.cmake)

# ── コンポーネント（各ディレクトリは独立。存在するものだけビルドする）──────
# 依存順に add_subdirectory する:
#   - i2c-hal は libsensor(ADS1115) より先に処理する。
#   - spi-hal は libadxl345 より先に処理する。
foreach(_component spi-hal i2c-hal gpio libsensor libadxl345 libmcp9808 cli examples)
    if(EXISTS ${CMAKE_SOURCE_DIR}/${_component}/CMakeLists.txt)
        add_subdirectory(${_component})
    endif()
endforeach()

# ── テスト（BUILD_TESTING=ON のとき、同一ツリーのターゲットに直接リンクして
#    ビルドする経路）────────────────────────────────────────────────────────
# CI 既定の「各ライブラリを install → テストをスタンドアロン configure（-I 手渡し）」
# 方式（落とし穴 #1/#2）とは別に、トップツリーから直接テストをビルドする経路を用意する。
# テストは同一ツリーの target（spihal / sensor / ads1115 …）に直接リンクでき、include は
# target_link_libraries の PUBLIC/INTERFACE から自動伝播するため CI の -I 列挙が不要になる
# （issue #20）。install 済みライブラリを名前解決する従来方式も価値があるため両立させる
# （こちらは BUILD_TESTING を使わず従来どおり）。
#
# 各テストディレクトリは「対応コンポーネントの target が存在するときだけ」追加し、
# コンポーネント削除耐性（ADR 0001）を保つ。
option(BUILD_TESTING "Build unit/integration tests in-tree (links in-tree targets, no -I handoff)" OFF)
if(BUILD_TESTING)
    enable_testing()

    if(TARGET spihal AND EXISTS ${CMAKE_SOURCE_DIR}/tests/unit/spi-hal/CMakeLists.txt)
        add_subdirectory(tests/unit/spi-hal)
    endif()
    if(TARGET i2chal AND EXISTS ${CMAKE_SOURCE_DIR}/tests/unit/i2c-hal/CMakeLists.txt)
        add_subdirectory(tests/unit/i2c-hal)
    endif()
    if(TARGET gpio AND EXISTS ${CMAKE_SOURCE_DIR}/tests/unit/gpio/CMakeLists.txt)
        add_subdirectory(tests/unit/gpio)
    endif()
    # libsensor のテストは test_sensor(sensor) と test_ads1115(ads1115) を含むため両 target を要求
    if(TARGET sensor AND TARGET ads1115 AND EXISTS ${CMAKE_SOURCE_DIR}/tests/unit/libsensor/CMakeLists.txt)
        add_subdirectory(tests/unit/libsensor)
    endif()
    if(TARGET adxl345 AND EXISTS ${CMAKE_SOURCE_DIR}/tests/unit/libadxl345/CMakeLists.txt)
        add_subdirectory(tests/unit/libadxl345)
    endif()
    if(TARGET mcp9808 AND EXISTS ${CMAKE_SOURCE_DIR}/tests/unit/libmcp9808/CMakeLists.txt)
        add_subdirectory(tests/unit/libmcp9808)
    endif()
    # 結合テストは sensor / adxl345 / ads1115 / gpio をリンクするため全 target を要求
    if(TARGET sensor AND TARGET adxl345 AND TARGET ads1115 AND TARGET gpio
       AND EXISTS ${CMAKE_SOURCE_DIR}/tests/integration/CMakeLists.txt)
        add_subdirectory(tests/integration)
    endif()
endif()
