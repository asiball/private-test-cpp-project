cmake_minimum_required(VERSION 3.10)
project(device-ctl VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# cli/src/main.cpp は sensor.hpp を無条件 include するため、libsensor(sensor) が
# 無い構成（TARGET sensor が無く、フォールバックのソースも無い）ではヘッダすら
# 解決できず全体ビルドが失敗する。ADR 0001（一部を削除してもビルドできる）に
# 従い、その場合は device-ctl 自体をスキップする（examples と同方針）。
if(TARGET sensor)
    add_executable(device-ctl src/main.cpp)
    target_link_libraries(device-ctl sensor)

    # logger.hpp（common/include）は sensor 経由の PUBLIC 伝播に頼らず、
    # cli 自身が include パスを通す（コンポーネント独立の原則）。
    target_include_directories(device-ctl PRIVATE ${CMAKE_SOURCE_DIR}/common/include)

    # version.hpp はビルドツリー（${CMAKE_BINARY_DIR}/generated）に生成されるため
    # その include パスを通す（生成物をソースツリーへ書き込まない）。
    target_include_directories(device-ctl PRIVATE ${CMAKE_BINARY_DIR}/generated)

    install(TARGETS device-ctl RUNTIME DESTINATION bin)
else()
    message(STATUS "cli: libsensor (dependency) not found; skipping device-ctl target")
endif()
