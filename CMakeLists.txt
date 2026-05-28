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

# version.hpp.in → spi-hal/include/version.hpp を生成
configure_file(
    ${CMAKE_SOURCE_DIR}/spi-hal/include/version.hpp.in
    ${CMAKE_SOURCE_DIR}/spi-hal/include/version.hpp
    @ONLY
)

add_subdirectory(spi-hal)
add_subdirectory(libsensor)
add_subdirectory(cli)
