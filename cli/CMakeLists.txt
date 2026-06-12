cmake_minimum_required(VERSION 3.10)
project(device-ctl VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(device-ctl src/main.cpp)

# libsensor(sensor) が無い構成では -lsensor 扱いになりリンク失敗するため、
# TARGET ガードで存在する場合のみリンクする（ADR 0001 / CLAUDE.md の
# 「一部を削除してもビルドできる」前提。examples と同方針）。
if(TARGET sensor)
    target_link_libraries(device-ctl sensor)
endif()

# version.hpp はビルドツリー（${CMAKE_BINARY_DIR}/generated）に生成されるため
# その include パスを通す（生成物をソースツリーへ書き込まない）。
target_include_directories(device-ctl PRIVATE ${CMAKE_BINARY_DIR}/generated)

install(TARGETS device-ctl RUNTIME DESTINATION bin)
