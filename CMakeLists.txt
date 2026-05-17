cmake_minimum_required(VERSION 3.10)
project(embedded-device-suite VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

get_git_tag("driver/v*" EDS_VERSION_DRIVER "driver/v0.0.0-unknown")
get_git_tag("lib/v*"    EDS_VERSION_LIB    "lib/v0.0.0-unknown")
get_git_tag("cli/v*"    EDS_VERSION_CLI    "cli/v0.0.0-unknown")

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

message(STATUS "Version: driver=${EDS_VERSION_DRIVER} lib=${EDS_VERSION_LIB} cli=${EDS_VERSION_CLI}")
message(STATUS "Commit:  ${EDS_GIT_COMMIT}  Date: ${EDS_BUILD_DATE}")

# version.hpp.in → driver/include/version.hpp を生成
configure_file(
    ${CMAKE_SOURCE_DIR}/driver/include/version.hpp.in
    ${CMAKE_SOURCE_DIR}/driver/include/version.hpp
    @ONLY
)

add_subdirectory(lib)
add_subdirectory(cli)
# driver は別途 kernel Makefile でビルドする
